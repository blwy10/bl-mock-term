#include "history.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>

namespace mockterm::md {

namespace {
constexpr int64_t kDayMs = 86'400'000;
}  // namespace

std::vector<Bar> GenerateHistory(const Instrument& inst, HistoryRange range,
                                 double last_price, int64_t now_ms) {
  int count = 0;
  int64_t step_ms = 0;
  double vol = 0;
  switch (range) {
    case HistoryRange::OneDay: count = 390; step_ms = 60'000; vol = 0.0009; break;
    case HistoryRange::OneMonth: count = 22 * 7; step_ms = 30 * kDayMs / (count - 1); vol = 0.004; break;
    case HistoryRange::OneYear: count = 252; step_ms = 365 * kDayMs / (count - 1); vol = 0.013; break;
  }
  if (inst.asset_class == AssetClass::Fx) vol *= 0.35;
  if (inst.asset_class == AssetClass::Index) vol *= 0.7;
  bool additive = inst.asset_class == AssetClass::Rate;
  if (additive) vol = (range == HistoryRange::OneDay ? 0.004 : range == HistoryRange::OneMonth ? 0.012 : 0.04);

  std::seed_seq seq{static_cast<uint32_t>(std::hash<std::string>{}(inst.ticker)),
                    static_cast<uint32_t>(range)};
  std::mt19937_64 rng(seq);
  std::normal_distribution<double> n(0.0, 1.0);
  std::uniform_real_distribution<double> u(0.0, 1.0);

  // Walk backwards from the current price so the series ends at |last_price|.
  std::vector<double> closes(count);
  double p = last_price;
  for (int i = count - 1; i >= 0; --i) {
    closes[i] = p;
    p = additive ? std::max(0.001, p - n(rng) * vol) : p / std::exp(n(rng) * vol);
  }

  double f = std::pow(10.0, inst.decimals);
  auto round = [f](double v) { return std::round(v * f) / f; };
  std::vector<Bar> bars(count);
  double prev = p;
  int64_t start = now_ms - static_cast<int64_t>(count - 1) * step_ms;
  for (int i = 0; i < count; ++i) {
    Bar& b = bars[i];
    b.ts = start + static_cast<int64_t>(i) * step_ms;
    b.open = round(prev);
    b.close = round(closes[i]);
    double wiggle = additive ? vol * 0.5 : std::abs(closes[i]) * vol * 0.5;
    b.high = round(std::max(b.open, b.close) + u(rng) * wiggle);
    b.low = round(std::min(b.open, b.close) - u(rng) * wiggle);
    b.volume = inst.asset_class == AssetClass::Equity
                   ? static_cast<int64_t>(50'000 + u(rng) * 450'000) * (range == HistoryRange::OneYear ? 60 : 1)
                   : 0;
    prev = closes[i];
  }
  return bars;
}

}  // namespace mockterm::md
