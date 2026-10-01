#include "platform/renderer_bridge.h"

#include <algorithm>
#include <vector>

#include "include/cef_v8.h"
#include "platform/ipc.h"

namespace mockterm::platform {

// Bridge registrations shared by every V8 context (main and isolated worlds)
// of one frame; the browser tracks news/chat listeners per frame.
struct FrameListeners {
  int news = 0;
  int chat = 0;
};

struct FrameBridgeState {
  CefRefPtr<CefFrame> frame;
  CefRefPtr<CefV8Context> context;
  std::shared_ptr<FrameListeners> listeners;
  std::map<int, CefRefPtr<CefV8Value>> tick_callbacks;
  std::map<int, CefRefPtr<CefV8Value>> news_callbacks;
  std::map<int, CefRefPtr<CefV8Value>> chat_callbacks;
  std::map<int, CefRefPtr<CefV8Value>> pending;  // promises by request id

  void Send(CefRefPtr<CefProcessMessage> msg) {
    if (frame && frame->IsValid()) frame->SendProcessMessage(PID_BROWSER, msg);
  }
};

namespace {

// Renderer-wide so ids stay unique across all contexts of a frame.
int g_next_id = 1;

void SendListen(FrameBridgeState& st, const char* name, const char* channel) {
  auto msg = CefProcessMessage::Create(name);
  msg->GetArgumentList()->SetString(0, channel);
  st.Send(msg);
}

bool StartsWith(const std::string& s, const std::string& prefix) {
  return s.compare(0, prefix.size(), prefix) == 0;
}

CefRefPtr<CefV8Value> ParseJson(CefRefPtr<CefV8Context> ctx, const CefString& json) {
  CefRefPtr<CefV8Value> json_obj = ctx->GetGlobal()->GetValue("JSON");
  CefRefPtr<CefV8Value> parse = json_obj ? json_obj->GetValue("parse") : nullptr;
  if (!parse || !parse->IsFunction()) return CefV8Value::CreateNull();
  CefV8ValueList args{CefV8Value::CreateString(json)};
  CefRefPtr<CefV8Value> result = parse->ExecuteFunctionWithContext(ctx, json_obj, args);
  return result ? result : CefV8Value::CreateNull();
}

void Invoke(CefRefPtr<CefV8Context> ctx, CefRefPtr<CefV8Value> fn, CefRefPtr<CefV8Value> arg) {
  if (!fn || !fn->IsFunction()) return;
  CefV8ValueList args{arg};
  fn->ExecuteFunctionWithContext(ctx, nullptr, args);
}

enum class Kind { kTicks, kNews, kChat };

// Returned from subscribe/onNews/onChat; removes the registration.
class UnsubscribeHandler : public CefV8Handler {
 public:
  UnsubscribeHandler(std::weak_ptr<FrameBridgeState> state, Kind kind, int id)
      : state_(std::move(state)), kind_(kind), id_(id) {}

  bool Execute(const CefString&, CefRefPtr<CefV8Value>, const CefV8ValueList&,
               CefRefPtr<CefV8Value>& retval, CefString&) override {
    retval = CefV8Value::CreateUndefined();
    auto st = state_.lock();
    if (!st || done_) return true;
    done_ = true;
    switch (kind_) {
      case Kind::kTicks: {
        st->tick_callbacks.erase(id_);
        auto msg = CefProcessMessage::Create(ipc::kUnsubscribe);
        msg->GetArgumentList()->SetInt(0, id_);
        st->Send(msg);
        break;
      }
      case Kind::kNews:
        if (st->news_callbacks.erase(id_) && --st->listeners->news == 0)
          SendListen(*st, ipc::kUnlisten, ipc::kChannelNews);
        break;
      case Kind::kChat:
        if (st->chat_callbacks.erase(id_) && --st->listeners->chat == 0)
          SendListen(*st, ipc::kUnlisten, ipc::kChannelChat);
        break;
    }
    return true;
  }

 private:
  std::weak_ptr<FrameBridgeState> state_;
  Kind kind_;
  int id_;
  bool done_ = false;
  IMPLEMENT_REFCOUNTING(UnsubscribeHandler);
};

class ApiHandler : public CefV8Handler {
 public:
  explicit ApiHandler(std::weak_ptr<FrameBridgeState> state) : state_(std::move(state)) {}

  bool Execute(const CefString& name, CefRefPtr<CefV8Value>, const CefV8ValueList& args,
               CefRefPtr<CefV8Value>& retval, CefString& exception) override {
    auto st = state_.lock();
    if (!st) {
      exception = "mockterm bridge is no longer attached";
      return true;
    }
    const std::string fn = name.ToString();
    if (fn == "subscribe") return Subscribe(st, args, retval, exception);
    if (fn == "onNews") return Listen(st, Kind::kNews, args, retval, exception);
    if (fn == "onChat") return Listen(st, Kind::kChat, args, retval, exception);
    if (fn == "getHistory") {
      if (args.size() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
        exception = "getHistory(ticker: string, range: '1D'|'1M'|'1Y')";
        return true;
      }
      retval = Request(st, ipc::kMethodHistory, args[0]->GetStringValue(), args[1]->GetStringValue());
      return true;
    }
    if (fn == "getNews") {
      CefString query;
      if (!args.empty() && args[0]->IsString()) query = args[0]->GetStringValue();
      retval = Request(st, ipc::kMethodNews, query, "");
      return true;
    }
    if (fn == "sendChat") {
      if (args.size() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
        exception = "sendChat(to: string, text: string)";
        return true;
      }
      retval = Request(st, ipc::kMethodChat, args[0]->GetStringValue(), args[1]->GetStringValue());
      return true;
    }
    if (fn == "getSecurities") {
      retval = Request(st, ipc::kMethodSecurities, "", "");
      return true;
    }
    if (fn == "openApp") {
      if (args.empty() || !args[0]->IsString()) {
        exception = "openApp(command: string)";
        return true;
      }
      auto msg = CefProcessMessage::Create(ipc::kOpenApp);
      msg->GetArgumentList()->SetString(0, args[0]->GetStringValue());
      st->Send(msg);
      retval = CefV8Value::CreateUndefined();
      return true;
    }
    return false;
  }

 private:
  bool Subscribe(const std::shared_ptr<FrameBridgeState>& st, const CefV8ValueList& args,
                 CefRefPtr<CefV8Value>& retval, CefString& exception) {
    if (args.size() < 2 || !args[0]->IsArray() || !args[1]->IsFunction()) {
      exception = "subscribe(tickers: string[], onTick: (t) => void)";
      return true;
    }
    CefRefPtr<CefListValue> tickers = CefListValue::Create();
    const int n = args[0]->GetArrayLength();
    for (int i = 0; i < n; ++i) {
      CefRefPtr<CefV8Value> v = args[0]->GetValue(i);
      if (v && v->IsString()) tickers->SetString(tickers->GetSize(), v->GetStringValue());
    }
    const int id = g_next_id++;
    st->tick_callbacks[id] = args[1];
    auto msg = CefProcessMessage::Create(ipc::kSubscribe);
    msg->GetArgumentList()->SetInt(0, id);
    msg->GetArgumentList()->SetList(1, tickers);
    st->Send(msg);
    retval = CefV8Value::CreateFunction("unsubscribe", new UnsubscribeHandler(state_, Kind::kTicks, id));
    return true;
  }

  bool Listen(const std::shared_ptr<FrameBridgeState>& st, Kind kind, const CefV8ValueList& args,
              CefRefPtr<CefV8Value>& retval, CefString& exception) {
    if (args.empty() || !args[0]->IsFunction()) {
      exception = "callback function required";
      return true;
    }
    auto& callbacks = kind == Kind::kNews ? st->news_callbacks : st->chat_callbacks;
    int& count = kind == Kind::kNews ? st->listeners->news : st->listeners->chat;
    if (count++ == 0) SendListen(*st, ipc::kListen, kind == Kind::kNews ? ipc::kChannelNews : ipc::kChannelChat);
    const int id = g_next_id++;
    callbacks[id] = args[0];
    retval = CefV8Value::CreateFunction("unsubscribe", new UnsubscribeHandler(state_, kind, id));
    return true;
  }

  CefRefPtr<CefV8Value> Request(const std::shared_ptr<FrameBridgeState>& st, const char* method,
                                const CefString& a, const CefString& b) {
    CefRefPtr<CefV8Value> promise = CefV8Value::CreatePromise();
    const int id = g_next_id++;
    st->pending[id] = promise;
    auto msg = CefProcessMessage::Create(ipc::kRequest);
    auto list = msg->GetArgumentList();
    list->SetInt(0, id);
    list->SetString(1, method);
    list->SetString(2, a);
    list->SetString(3, b);
    st->Send(msg);
    return promise;
  }

  std::weak_ptr<FrameBridgeState> state_;
  IMPLEMENT_REFCOUNTING(ApiHandler);
};

}  // namespace

RendererBridge::RendererBridge() = default;

void RendererBridge::OnContextCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefV8Context> context) {
  // Only first-party app pages get the bridge.
  if (!StartsWith(frame->GetURL().ToString(),
                  std::string(ipc::kScheme) + "://" + ipc::kAppsHost + "/")) {
    return;
  }

  // A frame can own several contexts (e.g. DevTools isolated worlds); each gets
  // its own state, all sharing the frame's listener counts.
  auto& states = frames_[frame->GetIdentifier().ToString()];
  auto state = std::make_shared<FrameBridgeState>();
  state->frame = frame;
  state->context = context;
  state->listeners = states.empty() ? std::make_shared<FrameListeners>() : states.front()->listeners;
  states.push_back(state);

  CefRefPtr<ApiHandler> handler = new ApiHandler(state);
  CefRefPtr<CefV8Value> api = CefV8Value::CreateObject(nullptr, nullptr);
  for (const char* fn : {"subscribe", "getHistory", "getNews", "onNews", "sendChat", "onChat",
                         "openApp", "getSecurities"}) {
    api->SetValue(fn, CefV8Value::CreateFunction(fn, handler), V8_PROPERTY_ATTRIBUTE_READONLY);
  }
  context->GetGlobal()->SetValue("mockterm", api,
                                 static_cast<cef_v8_propertyattribute_t>(
                                     V8_PROPERTY_ATTRIBUTE_READONLY | V8_PROPERTY_ATTRIBUTE_DONTDELETE));
}

void RendererBridge::OnContextReleased(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
                                       CefRefPtr<CefV8Context> context) {
  auto it = frames_.find(frame->GetIdentifier().ToString());
  if (it == frames_.end()) return;
  auto& states = it->second;
  auto pos = std::find_if(states.begin(), states.end(),
                          [&](const auto& st) { return st->context->IsSame(context); });
  if (pos == states.end()) return;
  auto state = *pos;
  states.erase(pos);
  for (const auto& [id, cb] : state->tick_callbacks) {
    auto msg = CefProcessMessage::Create(ipc::kUnsubscribe);
    msg->GetArgumentList()->SetInt(0, id);
    state->Send(msg);
  }
  auto& counts = *state->listeners;
  if (!state->news_callbacks.empty() && (counts.news -= static_cast<int>(state->news_callbacks.size())) == 0)
    SendListen(*state, ipc::kUnlisten, ipc::kChannelNews);
  if (!state->chat_callbacks.empty() && (counts.chat -= static_cast<int>(state->chat_callbacks.size())) == 0)
    SendListen(*state, ipc::kUnlisten, ipc::kChannelChat);
  if (states.empty()) frames_.erase(it);
}

bool RendererBridge::OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
                                              CefProcessId, CefRefPtr<CefProcessMessage> message) {
  auto it = frames_.find(frame->GetIdentifier().ToString());
  if (it == frames_.end()) return false;
  const auto states = it->second;  // copy: keeps states alive across JS callbacks
  const std::string name = message->GetName().ToString();
  CefRefPtr<CefListValue> args = message->GetArgumentList();

  if (name == ipc::kTicks) {
    for (const auto& st : states) {
      auto cb = st->tick_callbacks.find(args->GetInt(0));
      if (cb == st->tick_callbacks.end() || !st->context->IsValid()) continue;
      CefRefPtr<CefV8Value> fn = cb->second;
      CefRefPtr<CefV8Context> ctx = st->context;
      ctx->Enter();
      CefRefPtr<CefV8Value> ticks = ParseJson(ctx, args->GetString(1));
      if (ticks->IsArray()) {
        const int n = ticks->GetArrayLength();
        for (int i = 0; i < n; ++i) Invoke(ctx, fn, ticks->GetValue(i));
      }
      ctx->Exit();
    }
    return true;
  }
  if (name == ipc::kNews || name == ipc::kChat) {
    for (const auto& st : states) {
      auto& callbacks = name == ipc::kNews ? st->news_callbacks : st->chat_callbacks;
      if (callbacks.empty() || !st->context->IsValid()) continue;
      std::vector<CefRefPtr<CefV8Value>> fns;
      for (const auto& [id, fn] : callbacks) fns.push_back(fn);
      CefRefPtr<CefV8Context> ctx = st->context;
      ctx->Enter();
      CefRefPtr<CefV8Value> value = ParseJson(ctx, args->GetString(0));
      for (const auto& fn : fns) Invoke(ctx, fn, value);
      ctx->Exit();
    }
    return true;
  }
  if (name == ipc::kResponse) {
    for (const auto& st : states) {
      auto p = st->pending.find(args->GetInt(0));
      if (p == st->pending.end()) continue;
      CefRefPtr<CefV8Value> promise = p->second;
      st->pending.erase(p);
      if (!st->context->IsValid()) return true;
      CefRefPtr<CefV8Context> ctx = st->context;
      ctx->Enter();
      if (args->GetBool(1)) {
        promise->ResolvePromise(ParseJson(ctx, args->GetString(2)));
      } else {
        promise->RejectPromise(args->GetString(2));
      }
      ctx->Exit();
      return true;
    }
    return true;
  }
  return false;
}

}  // namespace mockterm::platform
