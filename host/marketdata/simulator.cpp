#include "simulator.h"

#include <algorithm>
#include <cmath>

namespace mockterm::md {

TickSimulator::TickSimulator(std::vector<Instrument> instruments, uint64_t seed)
    : instruments_(std::move(instruments)), rng_(seed) {
  state_.reserve(instruments_.size());
  for (size_t i = 0; i < instruments_.size(); ++i) {
    const Instrument& inst = instruments_[i];
    index_[inst.ticker] = i;
    // Start each instrument slightly away from its reference close so the
    // day opens with a non-zero change.
    double drift = inst.asset_class == AssetClass::Rate
                       ? normal_(rng_) * 0.02
                       : inst.base_price * normal_(rng_) * 0.004;
    Tick t;
    t.ticker = inst.ticker;
    t.prev_close = Round(inst.base_price, inst.decimals);
    t.last = Round(inst.base_price + drift, inst.decimals);
    t.open = t.last;
    t.high = t.open;
    t.low = t.open;
    double half = std::max(t.last * inst.spread_bps / 20000.0, std::pow(10.0, -inst.decimals));
    t.bid = Round(t.last - half, inst.decimals);
    t.ask = Round(t.last + half, inst.decimals);
    if (t.ask <= t.bid) t.ask = t.bid + std::pow(10.0, -inst.decimals);
    t.change = Round(t.last - t.prev_close, inst.decimals);
    t.change_pct = t.prev_close != 0 ? (t.last - t.prev_close) / t.prev_close * 100.0 : 0;
    t.volume = inst.asset_class == AssetClass::Equity ? 1'000'000 + static_cast<int64_t>(uniform_(rng_) * 4'000'000) : 0;
    state_.push_back(t);
  }
}

double TickSimulator::Round(double v, int decimals) const {
  double f = std::pow(10.0, decimals);
  return std::round(v * f) / f;
}

Tick TickSimulator::Advance(size_t i, int64_t now_ms) {
  const Instrument& inst = instruments_[i];
  Tick& t = state_[i];
  double step = normal_(rng_);
  double next;
  if (inst.asset_class == AssetClass::Rate) {
    next = t.last + step * inst.volatility;
    next = std::max(next, 0.001);
  } else {
    // Mild mean reversion towards the reference price keeps long sessions sane.
    double pull = (inst.base_price - t.last) / inst.base_price * 0.002;
    next = t.last * std::exp(step * inst.volatility + pull);
  }
  double tick_size = std::pow(10.0, -inst.decimals);
  next = std::max(Round(next, inst.decimals), tick_size);
  t.last = next;
  t.high = std::max(t.high, next);
  t.low = std::min(t.low, next);
  double half = std::max(next * inst.spread_bps / 20000.0, tick_size);
  t.bid = Round(next - half, inst.decimals);
  t.ask = Round(next + half, inst.decimals);
  if (t.ask <= t.bid) t.ask = Round(t.bid + tick_size, inst.decimals);
  t.change = Round(next - t.prev_close, inst.decimals);
  t.change_pct = t.prev_close != 0 ? (next - t.prev_close) / t.prev_close * 100.0 : 0;
  int64_t lot = 0;
  switch (inst.asset_class) {
    case AssetClass::Equity: lot = 100 * (1 + static_cast<int64_t>(uniform_(rng_) * 50)); break;
    case AssetClass::Index: lot = 0; break;
    case AssetClass::Fx: lot = 1'000'000 * (1 + static_cast<int64_t>(uniform_(rng_) * 10)); break;
    case AssetClass::Rate: lot = 0; break;
  }
  t.volume += lot;
  t.ts = now_ms;
  t.seq = ++seq_;
  return t;
}

std::vector<Tick> TickSimulator::Step(int64_t now_ms, double tick_probability) {
  std::vector<Tick> out;
  for (size_t i = 0; i < instruments_.size(); ++i) {
    if (uniform_(rng_) < tick_probability) out.push_back(Advance(i, now_ms));
  }
  return out;
}

std::optional<Tick> TickSimulator::TickOne(const std::string& ticker, int64_t now_ms) {
  auto it = index_.find(ticker);
  if (it == index_.end()) return std::nullopt;
  return Advance(it->second, now_ms);
}

std::optional<Tick> TickSimulator::Snapshot(const std::string& ticker) const {
  auto it = index_.find(ticker);
  if (it == index_.end()) return std::nullopt;
  return state_[it->second];
}

const Instrument* TickSimulator::Find(const std::string& ticker) const {
  auto it = index_.find(ticker);
  return it == index_.end() ? nullptr : &instruments_[it->second];
}

}  // namespace mockterm::md
