#include "convert.h"

#include <mtcore_element.h>

namespace mockterm::md::convert {

namespace {

template <typename T, typename Fn>
std::vector<T> ArrayOf(const mtcore::Element& parent, const char* name, Fn fn) {
  std::vector<T> out;
  const mtcore::Element* list = parent.getChild(name);
  if (!list) return out;
  out.reserve(list->numChildren());
  for (size_t i = 0; i < list->numChildren(); ++i) out.push_back(fn(*list->childAt(i)));
  return out;
}

void SetIf(const mtcore::Element& e, const char* name, double* out) {
  if (e.hasChild(name)) *out = e.getChildFloat64(name);
}

void SetIf(const mtcore::Element& e, const char* name, int64_t* out) {
  if (e.hasChild(name)) *out = e.getChildInt64(name);
}

}  // namespace

const std::vector<std::string>& TickFields() {
  static const std::vector<std::string> fields = {
      "LAST_PRICE", "BID",        "ASK",        "OPEN",   "HIGH",
      "LOW",        "PREV_CLOSE", "NET_CHANGE", "PCT_CHANGE", "VOLUME",
      "LAST_UPDATE_TIME", "SEQUENCE_NUMBER"};
  return fields;
}

void ApplyTickFields(Tick* tick, const mtcore::Element& f) {
  SetIf(f, "LAST_PRICE", &tick->last);
  SetIf(f, "BID", &tick->bid);
  SetIf(f, "ASK", &tick->ask);
  SetIf(f, "OPEN", &tick->open);
  SetIf(f, "HIGH", &tick->high);
  SetIf(f, "LOW", &tick->low);
  SetIf(f, "PREV_CLOSE", &tick->prev_close);
  SetIf(f, "NET_CHANGE", &tick->change);
  SetIf(f, "PCT_CHANGE", &tick->change_pct);
  SetIf(f, "VOLUME", &tick->volume);
  SetIf(f, "LAST_UPDATE_TIME", &tick->ts);
  if (f.hasChild("SEQUENCE_NUMBER")) tick->seq = static_cast<uint64_t>(f.getChildInt64("SEQUENCE_NUMBER"));
}

std::vector<Bar> BarsFromHistory(const mtcore::Element& response) {
  return ArrayOf<Bar>(response, "bars", [](const mtcore::Element& e) {
    Bar b;
    b.ts = e.getChildInt64("time");
    b.open = e.getChildFloat64("open");
    b.high = e.getChildFloat64("high");
    b.low = e.getChildFloat64("low");
    b.close = e.getChildFloat64("close");
    b.volume = e.getChildInt64("volume");
    return b;
  });
}

Headline HeadlineFrom(const mtcore::Element& e) {
  Headline h;
  h.id = static_cast<uint64_t>(e.getChildInt64("id"));
  h.ts = e.getChildInt64("time");
  h.source = e.getChildString("source");
  h.category = e.getChildString("category");
  h.tickers = ArrayOf<std::string>(e, "tickers", [](const mtcore::Element& t) { return t.getString(); });
  h.headline = e.getChildString("headline");
  h.body = e.getChildString("body");
  return h;
}

std::vector<Headline> HeadlinesFrom(const mtcore::Element& response) {
  return ArrayOf<Headline>(response, "headlines", HeadlineFrom);
}

ChatMessage ChatFrom(const mtcore::Element& e, const std::string& viewer) {
  ChatMessage m;
  m.id = static_cast<uint64_t>(e.getChildInt64("id"));
  m.ts = e.getChildInt64("time");
  m.from = e.getChildString("from");
  m.to = e.getChildString("to");
  m.text = e.getChildString("text");
  m.outgoing = m.from == viewer;
  return m;
}

Instrument InstrumentFrom(const mtcore::Element& e) {
  Instrument i;
  i.ticker = e.getChildString("ticker");
  i.name = e.getChildString("name");
  ParseAssetClass(e.getChildString("assetClass"), &i.asset_class);
  i.region = e.getChildString("region");
  i.currency = e.getChildString("currency");
  i.decimals = static_cast<int>(e.getChildInt64("decimals", 2));
  i.base_price = e.getChildFloat64("basePrice");
  if (const mtcore::Element* f = e.getChild("fundamentals")) {
    i.fundamentals.sector = f->getChildString("sector");
    i.fundamentals.market_cap_bn = f->getChildFloat64("marketCapBn");
    i.fundamentals.pe = f->getChildFloat64("pe");
    i.fundamentals.dividend_yield = f->getChildFloat64("dividendYield");
    i.fundamentals.eps = f->getChildFloat64("eps");
    i.fundamentals.beta = f->getChildFloat64("beta", 1);
    i.fundamentals.description = f->getChildString("description");
  }
  return i;
}

std::vector<Instrument> InstrumentsFrom(const mtcore::Element& response) {
  return ArrayOf<Instrument>(response, "securities", InstrumentFrom);
}

std::string ErrorText(const mtcore::Element& body) {
  const mtcore::Element* reason = body.getChild("reason");
  const mtcore::Element& r = reason ? *reason : body;
  std::string text = r.getChildString("message");
  std::string category = r.getChildString("category");
  if (text.empty()) text = "request failed";
  return category.empty() ? text : category + ": " + text;
}

}  // namespace mockterm::md::convert
