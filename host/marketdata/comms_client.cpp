#include "comms_client.h"

#include <mtcore_log.h>
#include <mtcore_message.h>
#include <mtcore_process.h>
#include <mtcore_socket.h>

#include <utility>

#include "convert.h"
#include "json.h"

namespace mockterm::md {

namespace {

constexpr char kMktData[] = "//mt/mktdata";
constexpr char kRefData[] = "//mt/refdata";
constexpr char kNewsSvc[] = "//mt/news";
constexpr char kMsgSvc[] = "//mt/msg";
constexpr char kNewsTopic[] = "HEADLINES";

mtclient::SessionOptions SessionOptionsFrom(const CommsOptions& o) {
  mtclient::SessionOptions s;
  s.host = o.host;
  s.port = o.port;
  s.user = o.user;
  s.application = o.application;
  s.conflateMs = o.conflate_ms;
  s.requestTimeoutMs = o.request_timeout_ms;
  s.autoReconnect = true;
  return s;
}

}  // namespace

CommsClient::CommsClient(CommsListener* listener, CommsOptions options)
    : listener_(listener), options_(std::move(options)) {
  session_ = std::make_unique<mtclient::Session>(SessionOptionsFrom(options_),
                                                 static_cast<mtclient::EventHandler*>(this));
}

CommsClient::~CommsClient() { Stop(); }

void CommsClient::Start() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (started_) return;
    started_ = true;
  }
  if (!options_.log_file.empty()) {
    mtcore::Logger::setProcessTag("mockterm");
    mtcore::Logger::setFile(options_.log_file.string().c_str());
  }
  mtcore::SocketUtil::initialize();
  if (options_.spawn && !CommsListening()) SpawnComms();

  news_cid_ = session_->nextCorrelationId();
  chat_cid_ = session_->nextCorrelationId();
  session_->subscribe(kNewsSvc, kNewsTopic, {}, news_cid_);
  session_->subscribe(kMsgSvc, options_.user, {}, chat_cid_);
  session_->start();
}

void CommsClient::Stop() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!started_) return;
    started_ = false;
  }
  session_->stop();
  std::map<uint64_t, Pending> pending;
  {
    std::lock_guard<std::mutex> lock(mu_);
    pending.swap(pending_);
    topics_.clear();
    by_subscriber_.clear();
  }
  for (auto& [cid, p] : pending) p.reply(false, "terminal shutting down");
  if (child_) {
    child_->terminate();
    child_.reset();
  }
  mtcore::SocketUtil::cleanup();
}

void CommsClient::Subscribe(SubscriberId id, const std::vector<std::string>& tickers) {
  std::vector<std::pair<uint64_t, std::string>> added;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& cids = by_subscriber_[id];
    for (const auto& ticker : tickers) {
      const uint64_t cid = session_->nextCorrelationId();
      Topic topic;
      topic.subscriber = id;
      topic.tick.ticker = ticker;
      topics_[cid] = std::move(topic);
      cids.push_back(cid);
      added.emplace_back(cid, ticker);
    }
  }
  for (const auto& [cid, ticker] : added) session_->subscribe(kMktData, ticker, convert::TickFields(), cid);
}

void CommsClient::Unsubscribe(SubscriberId id) {
  std::vector<uint64_t> cids;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = by_subscriber_.find(id);
    if (it == by_subscriber_.end()) return;
    cids = std::move(it->second);
    by_subscriber_.erase(it);
    for (uint64_t cid : cids) topics_.erase(cid);
  }
  for (uint64_t cid : cids) session_->unsubscribe(kMktData, cid);
}

void CommsClient::History(const std::string& ticker, HistoryRange range, Reply reply) {
  mtcore::Message req(mtcore::MessageType::e_REQUEST);
  req.setService(kRefData);
  req.setOperation("HistoricalData");
  req.body().setChildString("security", ticker);
  req.body().setChildString("range", ToString(range));
  Send(RequestKind::History, req, std::move(reply));
}

void CommsClient::News(const std::string& query, Reply reply) {
  mtcore::Message req(mtcore::MessageType::e_REQUEST);
  req.setService(kNewsSvc);
  req.setOperation("Search");
  req.body().setChildString("query", query);
  req.body().setChildInt64("maxResults", 50);
  Send(RequestKind::News, req, std::move(reply));
}

void CommsClient::Securities(Reply reply) {
  mtcore::Message req(mtcore::MessageType::e_REQUEST);
  req.setService(kRefData);
  req.setOperation("SecurityList");
  Send(RequestKind::Securities, req, std::move(reply));
}

void CommsClient::SendChat(const std::string& to, const std::string& text, Reply reply) {
  mtcore::Message req(mtcore::MessageType::e_REQUEST);
  req.setService(kMsgSvc);
  req.setOperation("Send");
  req.body().setChildString("from", options_.user);
  req.body().setChildString("to", to);
  req.body().setChildString("text", text);
  Send(RequestKind::Chat, req, std::move(reply));
}

ConnectionState CommsClient::state() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_;
}

void CommsClient::Send(RequestKind kind, mtcore::Message& request, Reply reply) {
  const uint64_t cid = session_->nextCorrelationId();
  request.setCorrelationId(cid);
  {
    std::lock_guard<std::mutex> lock(mu_);
    pending_[cid] = Pending{kind, std::move(reply)};
  }
  session_->sendRequest(request);
}

void CommsClient::processMessage(mtclient::Session*, const mtcore::Message& message) {
  switch (message.type()) {
    case mtcore::MessageType::e_RESPONSE:
    case mtcore::MessageType::e_REQUEST_FAILURE:
      HandleResponse(message);
      break;
    case mtcore::MessageType::e_SUBSCRIPTION_DATA:
      HandleData(message);
      break;
    case mtcore::MessageType::e_SUBSCRIPTION_FAILURE:
      MT_LOG_WARN("subscription %s %s failed: %s", message.service().c_str(), message.topic().c_str(),
                  convert::ErrorText(message.body()).c_str());
      break;
    case mtcore::MessageType::e_SESSION_STATUS: {
      const std::string status = message.body().getChildString("status");
      if (status == "UPSTREAM_DOWN") SetState(ConnectionState::Stale);
      if (status == "UPSTREAM_UP") SetState(ConnectionState::Live);
      break;
    }
    default:
      break;
  }
}

void CommsClient::processSessionState(mtclient::Session*, mtclient::SessionState::Value state,
                                      const mtcore::Message* logon_ack) {
  if (state == mtclient::SessionState::e_UP) {
    const bool upstream_up = logon_ack && logon_ack->body().getChildString("upstream", "UP") == "UP";
    SetState(upstream_up ? ConnectionState::Live : ConnectionState::Stale);
    return;
  }
  if (state == mtclient::SessionState::e_DOWN) {
    SetState(ConnectionState::Down);
    SuperviseComms();
  }
}

void CommsClient::HandleResponse(const mtcore::Message& message) {
  Pending p;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = pending_.find(message.correlationId());
    if (it == pending_.end()) return;
    p = std::move(it->second);
    pending_.erase(it);
  }
  if (message.type() == mtcore::MessageType::e_REQUEST_FAILURE) {
    p.reply(false, convert::ErrorText(message.body()));
    return;
  }
  const mtcore::Element& body = message.body();
  switch (p.kind) {
    case RequestKind::History:
      p.reply(true, json::ToJson(convert::BarsFromHistory(body)));
      break;
    case RequestKind::News:
      p.reply(true, json::ToJson(convert::HeadlinesFrom(body)));
      break;
    case RequestKind::Securities:
      p.reply(true, json::ToJson(convert::InstrumentsFrom(body)));
      break;
    case RequestKind::Chat: {
      const mtcore::Element* sent = body.getChild("message");
      if (!sent) {
        p.reply(false, "malformed chat response");
        break;
      }
      p.reply(true, json::ToJson(convert::ChatFrom(*sent, options_.user)));
      break;
    }
  }
}

void CommsClient::HandleData(const mtcore::Message& message) {
  const uint64_t cid = message.correlationId();
  if (cid == news_cid_) {
    listener_->OnNews(convert::HeadlineFrom(message.body()));
    return;
  }
  if (cid == chat_cid_) {
    listener_->OnChat(convert::ChatFrom(message.body(), options_.user));
    return;
  }
  SubscriberId subscriber = 0;
  Tick tick;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = topics_.find(cid);
    if (it == topics_.end()) return;
    convert::ApplyTickFields(&it->second.tick, message.body());
    subscriber = it->second.subscriber;
    tick = it->second.tick;
  }
  listener_->OnTicks(subscriber, {std::move(tick)});
}

void CommsClient::SetState(ConnectionState state) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (state_ == state) return;
    state_ = state;
  }
  MT_LOG_INFO("terminal connection state: %s", ToString(state));
  listener_->OnConnectionState(state);
}

bool CommsClient::CommsListening() const {
  mtcore::TcpSocket probe;
  return probe.connect(options_.host.c_str(), options_.port) == 0;
}

void CommsClient::SpawnComms() {
  std::filesystem::path exe = options_.comms_exe;
  if (exe.empty()) exe = std::filesystem::path(mtcore::ChildProcess::moduleDirectory()) / "mtcomm.exe";
  std::string cmd = mtcore::ChildProcess::quote(exe.string()) + " --port " + std::to_string(options_.port) +
                    " --spawn-server";
  for (const auto& arg : options_.comms_args) cmd += " " + mtcore::ChildProcess::quote(arg);
  auto child = std::make_unique<mtcore::ChildProcess>();
  if (child->start(cmd, exe.parent_path().string(), true) != 0) {
    MT_LOG_ERROR("failed to start %s", cmd.c_str());
    return;
  }
  MT_LOG_INFO("started mtcomm pid=%lu: %s", child->pid(), cmd.c_str());
  child_ = std::move(child);
}

void CommsClient::SuperviseComms() {
  if (!child_ || child_->isRunning()) return;
  MT_LOG_WARN("mtcomm exited (code %d); restarting", child_->wait(0));
  SpawnComms();
}

}  // namespace mockterm::md
