#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "chat.h"
#include "news.h"
#include "simulator.h"
#include "subscription_manager.h"
#include "types.h"

namespace mockterm::md {

// Callbacks are invoked on the service thread; implementations must marshal
// to their own thread (the platform layer posts to the CEF UI thread).
class MarketDataListener {
 public:
  virtual ~MarketDataListener() = default;
  virtual void OnTicks(SubscriberId subscriber, std::vector<Tick> ticks) = 0;
  virtual void OnNews(Headline headline) = 0;
  virtual void OnChat(ChatMessage message) = 0;
};

struct ServiceOptions {
  uint64_t seed = 0;            // 0 = seed from the clock
  int64_t loop_interval_ms = 25;
  int64_t throttle_ms = 100;    // max 10 Hz per subscription
  double tick_probability = 0.2;  // per instrument per loop
  int64_t news_min_ms = 2500;
  int64_t news_max_ms = 6000;
  int news_backfill = 20;
};

// Owns the simulator, subscriptions, news and chat, and runs them on a
// background thread. Public methods are thread-safe.
class MarketDataService {
 public:
  MarketDataService(MarketDataListener* listener, ServiceOptions options = {});
  ~MarketDataService();

  MarketDataService(const MarketDataService&) = delete;
  MarketDataService& operator=(const MarketDataService&) = delete;

  void Start();
  void Stop();

  // Unknown tickers are ignored; returns the tickers actually subscribed.
  std::vector<std::string> Subscribe(SubscriberId id, const std::vector<std::string>& tickers);
  void Unsubscribe(SubscriberId id);

  std::vector<Instrument> Instruments() const;
  std::optional<Instrument> FindInstrument(const std::string& ticker) const;
  std::optional<Tick> Snapshot(const std::string& ticker) const;
  std::optional<std::vector<Bar>> History(const std::string& ticker, HistoryRange range) const;
  std::vector<Headline> News(const std::string& query) const;
  ChatMessage SendChat(const std::string& to, const std::string& text);

  static int64_t NowMs();

 private:
  void Run();

  MarketDataListener* listener_;
  ServiceOptions options_;
  SubscriptionManager subs_;

  mutable std::mutex mu_;  // guards sim_, news_, chat_
  TickSimulator sim_;
  NewsGenerator news_;
  ChatBot chat_;
  int64_t next_news_ms_ = 0;
  std::mt19937_64 rng_;

  std::mutex wake_mu_;
  std::condition_variable wake_;
  std::atomic<bool> running_{false};
  std::thread thread_;
};

}  // namespace mockterm::md
