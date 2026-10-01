#pragma once

#include <cstdint>
#include <deque>
#include <random>
#include <string>
#include <vector>

#include "simulator.h"
#include "types.h"

namespace mockterm::md {

// Generates mock headlines referencing instruments in the universe and keeps
// a bounded in-memory archive. Not thread-safe.
class NewsGenerator {
 public:
  NewsGenerator(uint64_t seed, size_t capacity = 300);

  Headline Generate(const TickSimulator& sim, int64_t now_ms);

  // Newest first. Empty |query| returns everything; otherwise a
  // case-insensitive substring match on headline, body, source or tickers.
  std::vector<Headline> Search(const std::string& query, size_t limit = 100) const;

 private:
  std::mt19937_64 rng_;
  size_t capacity_;
  uint64_t next_id_ = 1;
  std::deque<Headline> archive_;  // oldest first
};

}  // namespace mockterm::md
