#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "history.h"
#include "json.h"
#include "news.h"
#include "chat.h"
#include "instruments.h"
#include "service.h"

using namespace mockterm::md;

namespace {
class Recorder : public MarketDataListener {
 public:
  void OnTicks(SubscriberId id, std::vector<Tick> ticks) override {
    std::lock_guard<std::mutex> l(mu);
    for (auto& t : ticks) tick_log.emplace_back(id, t);
    cv.notify_all();
  }
  void OnNews(Headline h) override {
    std::lock_guard<std::mutex> l(mu);
    news.push_back(h);
    cv.notify_all();
  }
  void OnChat(ChatMessage m) override {
    std::lock_guard<std::mutex> l(mu);
    chat.push_back(m);
    cv.notify_all();
  }
  template <typename Pred>
  bool WaitFor(Pred p, int ms = 3000) {
    std::unique_lock<std::mutex> l(mu);
    return cv.wait_for(l, std::chrono::milliseconds(ms), p);
  }
  std::mutex mu;
  std::condition_variable cv;
  std::vector<std::pair<SubscriberId, Tick>> tick_log;
  std::vector<Headline> news;
  std::vector<ChatMessage> chat;
};
}  // namespace

TEST(MarketDataService, StreamsSubscribedTicksAndSnapshots) {
  Recorder r;
  ServiceOptions o;
  o.seed = 5;
  o.tick_probability = 1.0;
  MarketDataService svc(&r, o);
  auto known = svc.Subscribe(7, {"AAPL US Equity", "BOGUS Equity"});
  ASSERT_EQ(known, std::vector<std::string>{"AAPL US Equity"});
  svc.Start();
  ASSERT_TRUE(r.WaitFor([&] { return r.tick_log.size() >= 3; }));
  svc.Stop();
  for (auto& [id, t] : r.tick_log) {
    EXPECT_EQ(id, 7u);
    EXPECT_EQ(t.ticker, "AAPL US Equity");
  }
}

TEST(MarketDataService, ChatGetsReplyQuotingTicker) {
  Recorder r;
  ServiceOptions o;
  o.seed = 6;
  MarketDataService svc(&r, o);
  svc.Start();
  ChatMessage sent = svc.SendChat("TRADER1", "where is msft?");
  EXPECT_TRUE(sent.outgoing);
  ASSERT_TRUE(r.WaitFor([&] { return !r.chat.empty(); }, 4000));
  svc.Stop();
  EXPECT_EQ(r.chat[0].from, "TRADER1");
  EXPECT_NE(r.chat[0].text.find("MSFT US Equity"), std::string::npos);
}

TEST(MarketDataService, HistoryEndsAtLastPrice) {
  MarketDataService svc(nullptr, ServiceOptions{});
  auto bars = svc.History("EURUSD Curncy", HistoryRange::OneDay);
  ASSERT_TRUE(bars.has_value());
  EXPECT_EQ(bars->size(), 390u);
  EXPECT_NEAR(bars->back().close, svc.Snapshot("EURUSD Curncy")->last, 1e-9);
  for (size_t i = 1; i < bars->size(); ++i) EXPECT_GT((*bars)[i].ts, (*bars)[i - 1].ts);
  for (const Bar& b : *bars) {
    EXPECT_LE(b.low, std::min(b.open, b.close));
    EXPECT_GE(b.high, std::max(b.open, b.close));
  }
  EXPECT_FALSE(svc.History("NOPE", HistoryRange::OneYear).has_value());
  EXPECT_EQ(svc.History("SPX Index", HistoryRange::OneYear)->size(), 252u);
}

TEST(MarketDataService, NewsArchiveIsBackfilledAndSearchable) {
  MarketDataService svc(nullptr, ServiceOptions{});
  auto all = svc.News("");
  ASSERT_GE(all.size(), 10u);
  for (size_t i = 1; i < all.size(); ++i) EXPECT_GE(all[i - 1].ts, all[i].ts);  // newest first
  const std::string& t = all[0].tickers.at(0);
  auto filtered = svc.News(t.substr(0, t.find(' ')));
  ASSERT_FALSE(filtered.empty());
  EXPECT_TRUE(svc.News("zzzz-no-match").empty());
}

TEST(Json, EscapesAndShapes) {
  EXPECT_EQ(json::Quote("a\"b\\c\n"), "\"a\\\"b\\\\c\\n\"");
  Tick t;
  t.ticker = "X";
  t.last = 1.5;
  std::string s = json::ToJson(t);
  EXPECT_NE(s.find("\"ticker\":\"X\""), std::string::npos);
  EXPECT_NE(s.find("\"last\":1.5"), std::string::npos);
  EXPECT_NE(s.find("\"prevClose\":"), std::string::npos);
}
