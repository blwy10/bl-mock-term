#include "json.h"

#include <cmath>
#include <cstdio>

namespace mockterm::md::json {

namespace {

std::string Num(double v) {
  if (!std::isfinite(v)) return "null";
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.10g", v);
  return buf;
}

std::string Num(int64_t v) { return std::to_string(v); }
std::string Num(uint64_t v) { return std::to_string(v); }

template <typename T>
std::string Array(const std::vector<T>& items) {
  std::string out = "[";
  for (size_t i = 0; i < items.size(); ++i) {
    if (i) out += ',';
    out += ToJson(items[i]);
  }
  out += ']';
  return out;
}

}  // namespace

std::string Quote(const std::string& s) {
  std::string out = "\"";
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  out += '"';
  return out;
}

std::string ToJson(const Tick& t) {
  return "{\"ticker\":" + Quote(t.ticker) + ",\"bid\":" + Num(t.bid) + ",\"ask\":" + Num(t.ask) +
         ",\"last\":" + Num(t.last) + ",\"open\":" + Num(t.open) + ",\"high\":" + Num(t.high) +
         ",\"low\":" + Num(t.low) + ",\"prevClose\":" + Num(t.prev_close) +
         ",\"change\":" + Num(t.change) + ",\"changePct\":" + Num(t.change_pct) +
         ",\"volume\":" + Num(t.volume) + ",\"ts\":" + Num(t.ts) + ",\"seq\":" + Num(t.seq) + "}";
}

std::string ToJson(const std::vector<Tick>& ticks) { return Array(ticks); }

std::string ToJson(const Bar& b) {
  return "{\"ts\":" + Num(b.ts) + ",\"open\":" + Num(b.open) + ",\"high\":" + Num(b.high) +
         ",\"low\":" + Num(b.low) + ",\"close\":" + Num(b.close) + ",\"volume\":" + Num(b.volume) + "}";
}

std::string ToJson(const std::vector<Bar>& bars) { return Array(bars); }

std::string ToJson(const Headline& h) {
  std::string tickers = "[";
  for (size_t i = 0; i < h.tickers.size(); ++i) {
    if (i) tickers += ',';
    tickers += Quote(h.tickers[i]);
  }
  tickers += ']';
  return "{\"id\":" + Num(h.id) + ",\"ts\":" + Num(h.ts) + ",\"source\":" + Quote(h.source) +
         ",\"category\":" + Quote(h.category) + ",\"tickers\":" + tickers +
         ",\"headline\":" + Quote(h.headline) + ",\"body\":" + Quote(h.body) + "}";
}

std::string ToJson(const std::vector<Headline>& hs) { return Array(hs); }

std::string ToJson(const ChatMessage& m) {
  return "{\"id\":" + Num(m.id) + ",\"ts\":" + Num(m.ts) + ",\"from\":" + Quote(m.from) +
         ",\"to\":" + Quote(m.to) + ",\"text\":" + Quote(m.text) +
         ",\"direction\":" + Quote(m.outgoing ? "out" : "in") + "}";
}

std::string ToJson(const Instrument& i) {
  std::string out = "{\"ticker\":" + Quote(i.ticker) + ",\"name\":" + Quote(i.name) +
                    ",\"assetClass\":" + Quote(ToString(i.asset_class)) + ",\"region\":" + Quote(i.region) +
                    ",\"currency\":" + Quote(i.currency) + ",\"decimals\":" + std::to_string(i.decimals);
  if (i.asset_class == AssetClass::Equity) {
    const Fundamentals& f = i.fundamentals;
    out += ",\"fundamentals\":{\"sector\":" + Quote(f.sector) + ",\"marketCapBn\":" + Num(f.market_cap_bn) +
           ",\"pe\":" + Num(f.pe) + ",\"dividendYield\":" + Num(f.dividend_yield) + ",\"eps\":" + Num(f.eps) +
           ",\"beta\":" + Num(f.beta) + ",\"description\":" + Quote(f.description) + "}";
  }
  out += "}";
  return out;
}

std::string ToJson(const std::vector<Instrument>& is) { return Array(is); }

}  // namespace mockterm::md::json
