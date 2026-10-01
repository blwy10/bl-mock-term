#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <mtclient_session.h>

#include "types.h"

namespace mtcore {
class ChildProcess;
}

namespace mockterm::md {

// Callbacks are invoked on the mtclient session thread; implementations must
// marshal to their own thread (the platform layer posts to the CEF UI thread).
class CommsListener {
 public:
  virtual ~CommsListener() = default;
  virtual void OnTicks(SubscriberId subscriber, std::vector<Tick> ticks) = 0;
  virtual void OnNews(Headline headline) = 0;
  virtual void OnChat(ChatMessage message) = 0;
  virtual void OnConnectionState(ConnectionState state) = 0;
};

struct CommsOptions {
  std::string host = "127.0.0.1";
  int port = 8194;
  std::string user = "ME";
  std::string application = "mockterm";
  int conflate_ms = 100;  // max 10 Hz per subscription, applied by mtcomm
  int request_timeout_ms = 15000;
  // Start mtcomm (which in turn starts mtserver) if nothing is listening on
  // |port|, and restart it if it exits.
  bool spawn = true;
  std::filesystem::path comms_exe;  // default: mtcomm.exe next to this exe
  std::vector<std::string> comms_args;
  std::filesystem::path log_file;  // mtclient log; empty = no file
};

// Completion for a request: |ok| with a JSON payload, or !ok with an error.
using Reply = std::function<void(bool ok, std::string payload)>;

// Terminal-side client of mtcomm. Owns one reconnecting mtclient::Session and
// translates the window.mockterm API into protocol requests/subscriptions.
// Public methods are thread-safe.
class CommsClient : private mtclient::EventHandler {
 public:
  CommsClient(CommsListener* listener, CommsOptions options);
  ~CommsClient() override;

  CommsClient(const CommsClient&) = delete;
  CommsClient& operator=(const CommsClient&) = delete;

  void Start();
  void Stop();

  void Subscribe(SubscriberId id, const std::vector<std::string>& tickers);
  void Unsubscribe(SubscriberId id);

  void History(const std::string& ticker, HistoryRange range, Reply reply);
  void News(const std::string& query, Reply reply);
  void Securities(Reply reply);
  // On success the payload is the sent ChatMessage as JSON.
  void SendChat(const std::string& to, const std::string& text, Reply reply);

  ConnectionState state() const;
  int port() const { return options_.port; }

 private:
  enum class RequestKind { History, News, Securities, Chat };

  struct Topic {
    SubscriberId subscriber = 0;
    Tick tick;
  };

  struct Pending {
    RequestKind kind;
    Reply reply;
  };

  void processMessage(mtclient::Session* session, const mtcore::Message& message) override;
  void processSessionState(mtclient::Session* session, mtclient::SessionState::Value state,
                           const mtcore::Message* logon_ack) override;

  void Send(RequestKind kind, mtcore::Message& request, Reply reply);
  void HandleResponse(const mtcore::Message& message);
  void HandleData(const mtcore::Message& message);
  void SetState(ConnectionState state);
  bool CommsListening() const;
  void SpawnComms();
  void SuperviseComms();

  CommsListener* listener_;
  CommsOptions options_;
  std::unique_ptr<mtclient::Session> session_;
  std::unique_ptr<mtcore::ChildProcess> child_;

  mutable std::mutex mu_;
  ConnectionState state_ = ConnectionState::Connecting;
  std::map<uint64_t, Topic> topics_;  // by correlation id
  std::map<SubscriberId, std::vector<uint64_t>> by_subscriber_;
  std::map<uint64_t, Pending> pending_;
  uint64_t news_cid_ = 0;
  uint64_t chat_cid_ = 0;
  bool started_ = false;
};

}  // namespace mockterm::md
