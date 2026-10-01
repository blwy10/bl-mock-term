// Integration test: CommsClient -> real mtcomm.exe -> real mtserver.exe on
// private ports.
#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "comms_client.h"

using namespace mockterm::md;
using namespace std::chrono_literals;

namespace {

constexpr int kCommsPort = 28194;
constexpr int kServerPort = 28195;

class Recorder : public CommsListener {
 public:
  void OnTicks(SubscriberId id, std::vector<Tick> ticks) override {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& t : ticks) ticks_[id].push_back(std::move(t));
    cv_.notify_all();
  }
  void OnNews(Headline) override {}
  void OnChat(ChatMessage m) override {
    std::lock_guard<std::mutex> lock(mu_);
    chats_.push_back(std::move(m));
    cv_.notify_all();
  }
  void OnConnectionState(ConnectionState s) override {
    std::lock_guard<std::mutex> lock(mu_);
    state_ = s;
    cv_.notify_all();
  }

  template <typename Pred>
  bool WaitFor(Pred pred, std::chrono::milliseconds timeout = 15s) {
    std::unique_lock<std::mutex> lock(mu_);
    return cv_.wait_for(lock, timeout, [&] { return pred(); });
  }

  std::mutex mu_;
  std::condition_variable cv_;
  ConnectionState state_ = ConnectionState::Connecting;
  std::map<SubscriberId, std::vector<Tick>> ticks_;
  std::vector<ChatMessage> chats_;
};

struct Result {
  std::mutex mu;
  std::condition_variable cv;
  bool done = false;
  bool ok = false;
  std::string payload;

  Reply Callback() {
    return [this](bool success, std::string text) {
      std::lock_guard<std::mutex> lock(mu);
      ok = success;
      payload = std::move(text);
      done = true;
      cv.notify_all();
    };
  }
  bool Wait() {
    std::unique_lock<std::mutex> lock(mu);
    return cv.wait_for(lock, 15s, [this] { return done; });
  }
};

class CommsClientTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    CommsOptions o;
    o.port = kCommsPort;
    o.comms_exe = MTCOMM_EXE;
    o.comms_args = {"--upstream-port", std::to_string(kServerPort)};
    o.conflate_ms = 0;
    recorder_ = new Recorder();
    client_ = new CommsClient(recorder_, o);
    client_->Start();
  }
  static void TearDownTestSuite() {
    client_->Stop();
    delete client_;
    delete recorder_;
  }
  void SetUp() override {
    ASSERT_TRUE(recorder_->WaitFor([] { return recorder_->state_ == ConnectionState::Live; }, 30s))
        << "mtcomm/mtserver did not come up";
  }

  static Recorder* recorder_;
  static CommsClient* client_;
};

Recorder* CommsClientTest::recorder_ = nullptr;
CommsClient* CommsClientTest::client_ = nullptr;

}  // namespace

TEST_F(CommsClientTest, SecuritiesComeFromServerSecurityMaster) {
  Result r;
  client_->Securities(r.Callback());
  ASSERT_TRUE(r.Wait());
  ASSERT_TRUE(r.ok) << r.payload;
  EXPECT_NE(r.payload.find("\"ticker\":\"AAPL US Equity\""), std::string::npos);
  EXPECT_NE(r.payload.find("\"assetClass\":\"Curncy\""), std::string::npos);
}

TEST_F(CommsClientTest, HistoryReturnsBars) {
  Result r;
  client_->History("MSFT US Equity", HistoryRange::OneMonth, r.Callback());
  ASSERT_TRUE(r.Wait());
  ASSERT_TRUE(r.ok) << r.payload;
  EXPECT_NE(r.payload.find("\"close\":"), std::string::npos);
}

TEST_F(CommsClientTest, UnknownSecurityFails) {
  Result r;
  client_->History("NOPE US Equity", HistoryRange::OneDay, r.Callback());
  ASSERT_TRUE(r.Wait());
  EXPECT_FALSE(r.ok);
  EXPECT_FALSE(r.payload.empty());
}

TEST_F(CommsClientTest, SubscriptionStreamsFullTicksUntilUnsubscribed) {
  const SubscriberId id = 77;
  client_->Subscribe(id, {"EURUSD Curncy", "AAPL US Equity"});
  ASSERT_TRUE(recorder_->WaitFor([&] {
    auto it = recorder_->ticks_.find(id);
    if (it == recorder_->ticks_.end()) return false;
    bool fx = false, eq = false;
    for (const Tick& t : it->second) {
      fx |= t.ticker == "EURUSD Curncy";
      eq |= t.ticker == "AAPL US Equity";
    }
    return fx && eq && it->second.size() >= 4;
  }));
  {
    std::lock_guard<std::mutex> lock(recorder_->mu_);
    for (const Tick& t : recorder_->ticks_[id]) {
      EXPECT_GT(t.last, 0) << t.ticker;
      EXPECT_GT(t.ask, 0) << "partial updates are merged onto the initial paint";
    }
  }
  client_->Unsubscribe(id);
  std::this_thread::sleep_for(500ms);
  size_t before;
  {
    std::lock_guard<std::mutex> lock(recorder_->mu_);
    before = recorder_->ticks_[id].size();
  }
  std::this_thread::sleep_for(1s);
  std::lock_guard<std::mutex> lock(recorder_->mu_);
  EXPECT_EQ(recorder_->ticks_[id].size(), before);
}

TEST_F(CommsClientTest, ChatSendEchoesMessageAndBotReplies) {
  Result r;
  client_->SendChat("ALICE", "ping", r.Callback());
  ASSERT_TRUE(r.Wait());
  ASSERT_TRUE(r.ok) << r.payload;
  EXPECT_NE(r.payload.find("\"direction\":\"out\""), std::string::npos);
  EXPECT_TRUE(recorder_->WaitFor([] {
    for (const ChatMessage& m : recorder_->chats_) {
      if (m.from == "ALICE" && !m.outgoing) return true;
    }
    return false;
  }));
}
