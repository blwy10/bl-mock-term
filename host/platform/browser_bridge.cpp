#include "platform/browser_bridge.h"

#include "include/cef_frame.h"
#include "json.h"
#include "platform/ipc.h"
#include "platform/ui_task.h"

namespace mockterm::platform {

namespace {
std::string JsStringLiteral(const std::string& s) { return md::json::Quote(s); }
}  // namespace

BrowserBridge::BrowserBridge() : service_(std::make_unique<md::MarketDataService>(this)) {}

BrowserBridge::~BrowserBridge() { Shutdown(); }

void BrowserBridge::Start() { service_->Start(); }

void BrowserBridge::Shutdown() {
  if (service_) service_->Stop();
}

void BrowserBridge::OnBrowserCreated(CefRefPtr<CefBrowser> browser) {
  browsers_[browser->GetIdentifier()] = browser;
}

void BrowserBridge::OnBrowserClosed(CefRefPtr<CefBrowser> browser) {
  const int id = browser->GetIdentifier();
  std::vector<FrameKey> keys;
  for (const auto& [sub, s] : subs_) {
    if (s.frame.first == id) keys.push_back(s.frame);
  }
  for (const auto& k : news_listeners_) {
    if (k.first == id) keys.push_back(k);
  }
  for (const auto& k : chat_listeners_) {
    if (k.first == id) keys.push_back(k);
  }
  for (const auto& k : keys) DropFrame(k);
  browsers_.erase(id);
}

void BrowserBridge::OnFrameDetached(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
  DropFrame({browser->GetIdentifier(), frame->GetIdentifier().ToString()});
}

void BrowserBridge::DropFrame(const FrameKey& key) {
  for (auto it = subs_.begin(); it != subs_.end();) {
    if (it->second.frame == key) {
      service_->Unsubscribe(it->first);
      subs_by_js_.erase({key, it->second.js_id});
      it = subs_.erase(it);
    } else {
      ++it;
    }
  }
  news_listeners_.erase(key);
  chat_listeners_.erase(key);
}

CefRefPtr<CefFrame> BrowserBridge::FindFrame(const FrameKey& key) {
  auto b = browsers_.find(key.first);
  if (b == browsers_.end()) return nullptr;
  CefRefPtr<CefFrame> frame = b->second->GetFrameByIdentifier(key.second);
  if (!frame || !frame->IsValid()) return nullptr;
  return frame;
}

bool BrowserBridge::OnProcessMessage(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                     CefRefPtr<CefProcessMessage> message) {
  const std::string name = message->GetName().ToString();
  CefRefPtr<CefListValue> args = message->GetArgumentList();
  const FrameKey key{browser->GetIdentifier(), frame->GetIdentifier().ToString()};

  if (name == ipc::kSubscribe) {
    const int js_id = args->GetInt(0);
    std::vector<std::string> tickers;
    CefRefPtr<CefListValue> list = args->GetList(1);
    for (size_t i = 0; list && i < list->GetSize(); ++i) tickers.push_back(list->GetString(i).ToString());
    const md::SubscriberId id = next_subscriber_++;
    subs_[id] = Subscription{key, js_id};
    subs_by_js_[{key, js_id}] = id;
    service_->Subscribe(id, tickers);
    return true;
  }
  if (name == ipc::kUnsubscribe) {
    auto it = subs_by_js_.find({key, args->GetInt(0)});
    if (it != subs_by_js_.end()) {
      service_->Unsubscribe(it->second);
      subs_.erase(it->second);
      subs_by_js_.erase(it);
    }
    return true;
  }
  if (name == ipc::kListen || name == ipc::kUnlisten) {
    const std::string channel = args->GetString(0).ToString();
    auto& set = channel == ipc::kChannelNews ? news_listeners_ : chat_listeners_;
    if (name == ipc::kListen) {
      set.insert(key);
    } else {
      set.erase(key);
    }
    return true;
  }
  if (name == ipc::kRequest) {
    HandleRequest(frame, args);
    return true;
  }
  if (name == ipc::kOpenApp) {
    // The shell (main frame) owns the panel layout; hand the command to it.
    CefRefPtr<CefFrame> main = browser->GetMainFrame();
    if (main) {
      const std::string js = "window.dispatchEvent(new CustomEvent('mockterm:open',{detail:" +
                             JsStringLiteral(args->GetString(0).ToString()) + "}));";
      main->ExecuteJavaScript(js, main->GetURL(), 0);
    }
    return true;
  }
  return false;
}

void BrowserBridge::HandleRequest(CefRefPtr<CefFrame> frame, CefRefPtr<CefListValue> args) {
  const int req_id = args->GetInt(0);
  const std::string method = args->GetString(1).ToString();
  const std::string a = args->GetString(2).ToString();
  const std::string b = args->GetString(3).ToString();

  if (method == ipc::kMethodHistory) {
    md::HistoryRange range;
    if (!md::ParseHistoryRange(b, &range)) return Respond(frame, req_id, false, "invalid range: " + b);
    auto bars = service_->History(a, range);
    if (!bars) return Respond(frame, req_id, false, "unknown security: " + a);
    return Respond(frame, req_id, true, md::json::ToJson(*bars));
  }
  if (method == ipc::kMethodNews) return Respond(frame, req_id, true, md::json::ToJson(service_->News(a)));
  if (method == ipc::kMethodSecurities) {
    return Respond(frame, req_id, true, md::json::ToJson(service_->Instruments()));
  }
  if (method == ipc::kMethodChat) {
    if (b.empty()) return Respond(frame, req_id, false, "empty message");
    md::ChatMessage sent = service_->SendChat(a, b);
    Broadcast(chat_listeners_, ipc::kChat, md::json::ToJson(sent));
    return Respond(frame, req_id, true, "null");
  }
  Respond(frame, req_id, false, "unknown method: " + method);
}

void BrowserBridge::Respond(CefRefPtr<CefFrame> frame, int req_id, bool ok, const std::string& payload) {
  if (!frame || !frame->IsValid()) return;
  auto msg = CefProcessMessage::Create(ipc::kResponse);
  auto list = msg->GetArgumentList();
  list->SetInt(0, req_id);
  list->SetBool(1, ok);
  list->SetString(2, payload);
  frame->SendProcessMessage(PID_RENDERER, msg);
}

void BrowserBridge::DeliverTicks(md::SubscriberId subscriber, const std::string& json) {
  auto it = subs_.find(subscriber);
  if (it == subs_.end()) return;
  CefRefPtr<CefFrame> frame = FindFrame(it->second.frame);
  if (!frame) return;
  auto msg = CefProcessMessage::Create(ipc::kTicks);
  msg->GetArgumentList()->SetInt(0, it->second.js_id);
  msg->GetArgumentList()->SetString(1, json);
  frame->SendProcessMessage(PID_RENDERER, msg);
}

void BrowserBridge::Broadcast(std::set<FrameKey>& listeners, const char* name, const std::string& json) {
  std::vector<FrameKey> dead;
  for (const auto& key : listeners) {
    CefRefPtr<CefFrame> frame = FindFrame(key);
    if (!frame) {
      dead.push_back(key);
      continue;
    }
    auto msg = CefProcessMessage::Create(name);
    msg->GetArgumentList()->SetString(0, json);
    frame->SendProcessMessage(PID_RENDERER, msg);
  }
  for (const auto& key : dead) listeners.erase(key);
}

void BrowserBridge::OnTicks(md::SubscriberId subscriber, std::vector<md::Tick> ticks) {
  std::weak_ptr<BrowserBridge> weak = weak_from_this();
  std::string json = md::json::ToJson(ticks);
  PostUiTask([weak, subscriber, json = std::move(json)] {
    if (auto self = weak.lock()) self->DeliverTicks(subscriber, json);
  });
}

void BrowserBridge::OnNews(md::Headline headline) {
  std::weak_ptr<BrowserBridge> weak = weak_from_this();
  std::string json = md::json::ToJson(headline);
  PostUiTask([weak, json = std::move(json)] {
    if (auto self = weak.lock()) self->Broadcast(self->news_listeners_, ipc::kNews, json);
  });
}

void BrowserBridge::OnChat(md::ChatMessage message) {
  std::weak_ptr<BrowserBridge> weak = weak_from_this();
  std::string json = md::json::ToJson(message);
  PostUiTask([weak, json = std::move(json)] {
    if (auto self = weak.lock()) self->Broadcast(self->chat_listeners_, ipc::kChat, json);
  });
}

}  // namespace mockterm::platform
