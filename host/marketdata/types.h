#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mockterm::md {

using SubscriberId = uint64_t;

enum class AssetClass { Equity, Fx, Index, Rate };

const char* ToString(AssetClass c);

struct Fundamentals {
  std::string sector;
  double market_cap_bn = 0;  // USD billions
  double pe = 0;
  double dividend_yield = 0;  // percent
  double eps = 0;
  double beta = 1;
  std::string description;
};

struct Instrument {
  std::string ticker;  // canonical, e.g. "AAPL US Equity"
  std::string name;
  AssetClass asset_class = AssetClass::Equity;
  std::string region;  // Americas / EMEA / APAC
  std::string currency;
  double base_price = 0;
  double volatility = 0;  // relative std-dev per tick (absolute bp for rates)
  int decimals = 2;
  double spread_bps = 2;  // full bid/ask spread, basis points of price
  Fundamentals fundamentals;
};

struct Tick {
  std::string ticker;
  double bid = 0;
  double ask = 0;
  double last = 0;
  double open = 0;
  double high = 0;
  double low = 0;
  double prev_close = 0;
  double change = 0;
  double change_pct = 0;
  int64_t volume = 0;
  int64_t ts = 0;  // ms since epoch
  uint64_t seq = 0;
};

struct Bar {
  int64_t ts = 0;
  double open = 0;
  double high = 0;
  double low = 0;
  double close = 0;
  int64_t volume = 0;
};

struct Headline {
  uint64_t id = 0;
  int64_t ts = 0;
  std::string source;
  std::string category;
  std::vector<std::string> tickers;
  std::string headline;
  std::string body;
};

struct ChatMessage {
  uint64_t id = 0;
  int64_t ts = 0;
  std::string from;
  std::string to;
  std::string text;
  bool outgoing = false;
};

enum class HistoryRange { OneDay, OneMonth, OneYear };

bool ParseHistoryRange(const std::string& s, HistoryRange* out);
const char* ToString(HistoryRange r);

bool ParseAssetClass(const std::string& s, AssetClass* out);

// Health of the path mockterm -> mtcomm -> mtserver.
enum class ConnectionState {
  Connecting,  // no session to mtcomm yet
  Live,        // mtcomm up and its upstream to mtserver up
  Stale,       // mtcomm up, upstream down: cached values only
  Down,        // lost the session to mtcomm; reconnecting
};

const char* ToString(ConnectionState s);

}  // namespace mockterm::md
