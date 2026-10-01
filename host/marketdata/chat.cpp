#include "chat.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace mockterm::md {

const char* ChatBot::kDefaultUser = "ME";

namespace {

std::string Upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return s;
}

const char* kCanned[] = {
    "copy that, will revert shortly",
    "seeing decent two-way flow here, nothing crazy",
    "market feels heavy into the close tbh",
    "can work that for you, what size?",
    "noted. desk is a bit thin today",
    "agree, let's touch base after the data",
    "lol yes. anyway, levels look fine from here",
};

}  // namespace

ChatBot::ChatBot(uint64_t seed) : rng_(seed) {}

ChatMessage ChatBot::Send(const std::string& to, const std::string& text, int64_t now_ms) {
  std::uniform_int_distribution<int> delay(700, 2200);
  pending_.push_back({now_ms + delay(rng_), to, text});
  ChatMessage m;
  m.id = next_id_++;
  m.ts = now_ms;
  m.from = kDefaultUser;
  m.to = to;
  m.text = text;
  m.outgoing = true;
  return m;
}

std::vector<ChatMessage> ChatBot::Poll(const TickSimulator& sim, int64_t now_ms) {
  std::vector<ChatMessage> out;
  auto due = std::stable_partition(pending_.begin(), pending_.end(),
                                   [now_ms](const Pending& p) { return p.due_ms > now_ms; });
  for (auto it = due; it != pending_.end(); ++it) {
    ChatMessage m;
    m.id = next_id_++;
    m.ts = now_ms;
    m.from = it->counterparty;
    m.to = kDefaultUser;
    std::string prompt = Upper(it->prompt);
    std::string reply;
    for (const auto& inst : sim.instruments()) {
      std::string sym = inst.ticker.substr(0, inst.ticker.find(' '));
      if (prompt.find(sym) == std::string::npos) continue;
      auto snap = sim.Snapshot(inst.ticker);
      if (!snap) continue;
      char buf[256];
      std::snprintf(buf, sizeof buf, "%s showing %.*f / %.*f, last %.*f (%+.2f%%). want me to work an order?",
                    inst.ticker.c_str(), inst.decimals, snap->bid, inst.decimals, snap->ask,
                    inst.decimals, snap->last, snap->change_pct);
      reply = buf;
      break;
    }
    if (reply.empty()) {
      if (prompt.find("HI") == 0 || prompt.find("HELLO") == 0 || prompt.find("MORNING") != std::string::npos) {
        reply = "morning! what can I do for you?";
      } else {
        std::uniform_int_distribution<size_t> pick(0, std::size(kCanned) - 1);
        reply = kCanned[pick(rng_)];
      }
    }
    m.text = reply;
    out.push_back(std::move(m));
  }
  pending_.erase(due, pending_.end());
  return out;
}

}  // namespace mockterm::md
