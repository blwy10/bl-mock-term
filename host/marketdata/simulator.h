#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "types.h"

namespace mockterm::md {

// Random-walk tick generator. Not thread-safe; owned by the service thread.
class TickSimulator {
 public:
  TickSimulator(std::vector<Instrument> instruments, uint64_t seed);

  // Advances the simulation to |now_ms|. Each instrument ticks with
  // probability |tick_probability|; returns the ticks generated.
  std::vector<Tick> Step(int64_t now_ms, double tick_probability = 0.25);

  // Forces a single tick for |ticker|. Returns nullopt for unknown tickers.
  std::optional<Tick> TickOne(const std::string& ticker, int64_t now_ms);

  std::optional<Tick> Snapshot(const std::string& ticker) const;
  const Instrument* Find(const std::string& ticker) const;
  const std::vector<Instrument>& instruments() const { return instruments_; }

 private:
  Tick Advance(size_t index, int64_t now_ms);
  double Round(double v, int decimals) const;

  std::vector<Instrument> instruments_;
  std::vector<Tick> state_;
  std::unordered_map<std::string, size_t> index_;
  std::mt19937_64 rng_;
  std::normal_distribution<double> normal_{0.0, 1.0};
  std::uniform_real_distribution<double> uniform_{0.0, 1.0};
  uint64_t seq_ = 0;
};

}  // namespace mockterm::md
