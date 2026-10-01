#include "subscription_manager.h"

namespace mockterm::md {

SubscriptionManager::SubscriptionManager(int64_t min_interval_ms)
    : min_interval_ms_(min_interval_ms) {}

void SubscriptionManager::Subscribe(SubscriberId id, const std::vector<std::string>& tickers) {
  std::lock_guard<std::mutex> lock(mu_);
  Subscriber& s = subscribers_[id];
  for (const auto& t : tickers) {
    s.tickers.insert(t);
    by_ticker_[t].insert(id);
  }
}

void SubscriptionManager::Unsubscribe(SubscriberId id, const std::vector<std::string>& tickers) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = subscribers_.find(id);
  if (it == subscribers_.end()) return;
  for (const auto& t : tickers) {
    it->second.tickers.erase(t);
    it->second.pending.erase(t);
    auto bt = by_ticker_.find(t);
    if (bt != by_ticker_.end()) {
      bt->second.erase(id);
      if (bt->second.empty()) by_ticker_.erase(bt);
    }
  }
  if (it->second.tickers.empty()) subscribers_.erase(it);
}

void SubscriptionManager::RemoveSubscriber(SubscriberId id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = subscribers_.find(id);
  if (it == subscribers_.end()) return;
  for (const auto& t : it->second.tickers) {
    auto bt = by_ticker_.find(t);
    if (bt != by_ticker_.end()) {
      bt->second.erase(id);
      if (bt->second.empty()) by_ticker_.erase(bt);
    }
  }
  subscribers_.erase(it);
}

void SubscriptionManager::OnTick(const Tick& tick) {
  std::lock_guard<std::mutex> lock(mu_);
  auto bt = by_ticker_.find(tick.ticker);
  if (bt == by_ticker_.end()) return;
  for (SubscriberId id : bt->second) subscribers_[id].pending[tick.ticker] = tick;
}

void SubscriptionManager::Seed(SubscriberId id, const Tick& tick) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = subscribers_.find(id);
  if (it == subscribers_.end() || !it->second.tickers.count(tick.ticker)) return;
  it->second.pending[tick.ticker] = tick;
}

std::vector<Delivery> SubscriptionManager::Collect(int64_t now_ms) {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<Delivery> out;
  for (auto& [id, s] : subscribers_) {
    if (s.pending.empty()) continue;
    if (now_ms - s.last_delivery_ms < min_interval_ms_) continue;
    Delivery d;
    d.subscriber = id;
    d.ticks.reserve(s.pending.size());
    for (auto& [ticker, tick] : s.pending) d.ticks.push_back(std::move(tick));
    s.pending.clear();
    s.last_delivery_ms = now_ms;
    out.push_back(std::move(d));
  }
  return out;
}

size_t SubscriptionManager::SubscriberCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return subscribers_.size();
}

std::set<std::string> SubscriptionManager::ActiveTickers() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::set<std::string> out;
  for (const auto& [t, ids] : by_ticker_) out.insert(t);
  return out;
}

std::set<std::string> SubscriptionManager::TickersFor(SubscriberId id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = subscribers_.find(id);
  return it == subscribers_.end() ? std::set<std::string>{} : it->second.tickers;
}

}  // namespace mockterm::md
