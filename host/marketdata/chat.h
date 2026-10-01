#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "simulator.h"
#include "types.h"

namespace mockterm::md {

// A scripted IB counterparty: every outgoing message schedules one reply
// after a short, randomised delay. Not thread-safe.
class ChatBot {
 public:
  explicit ChatBot(uint64_t seed);

  // Records an outgoing message and schedules the counterparty's reply.
  ChatMessage Send(const std::string& to, const std::string& text, int64_t now_ms);

  // Returns replies whose delay has elapsed. |sim| is used to quote prices
  // when the user mentions a known ticker.
  std::vector<ChatMessage> Poll(const TickSimulator& sim, int64_t now_ms);

  static const char* kDefaultUser;

 private:
  struct Pending {
    int64_t due_ms;
    std::string counterparty;
    std::string prompt;
  };
  std::mt19937_64 rng_;
  uint64_t next_id_ = 1;
  std::vector<Pending> pending_;
};

}  // namespace mockterm::md
