#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "types.h"

namespace mockterm::md {

using SubscriberId = uint64_t;

struct Delivery {
  SubscriberId subscriber = 0;
  std::vector<Tick> ticks;  // at most one (the latest) tick per ticker
};

// Tracks which subscribers want which tickers and coalesces updates so each
// subscriber receives at most one batch per |min_interval_ms| (10 Hz by
// default). Thread-safe.
class SubscriptionManager {
 public:
  explicit SubscriptionManager(int64_t min_interval_ms = 100);

  void Subscribe(SubscriberId id, const std::vector<std::string>& tickers);
  void Unsubscribe(SubscriberId id, const std::vector<std::string>& tickers);
  void RemoveSubscriber(SubscriberId id);

  // Records |tick| for every subscriber of tick.ticker, replacing any
  // pending (not yet delivered) tick for the same ticker.
  void OnTick(const Tick& tick);

  // Queues |tick| for a single subscriber only (used for initial snapshots).
  void Seed(SubscriberId id, const Tick& tick);

  // Returns pending batches for subscribers whose throttle window elapsed.
  std::vector<Delivery> Collect(int64_t now_ms);

  size_t SubscriberCount() const;
  std::set<std::string> ActiveTickers() const;
  std::set<std::string> TickersFor(SubscriberId id) const;

 private:
  struct Subscriber {
    std::set<std::string> tickers;
    std::map<std::string, Tick> pending;  // ordered for stable output
    int64_t last_delivery_ms = INT64_MIN / 2;
  };

  mutable std::mutex mu_;
  int64_t min_interval_ms_;
  std::unordered_map<SubscriberId, Subscriber> subscribers_;
  std::unordered_map<std::string, std::set<SubscriberId>> by_ticker_;
};

}  // namespace mockterm::md
