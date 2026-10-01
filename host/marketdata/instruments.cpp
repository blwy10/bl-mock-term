#include "instruments.h"

namespace mockterm::md {

const char* ToString(AssetClass c) {
  switch (c) {
    case AssetClass::Equity: return "Equity";
    case AssetClass::Fx: return "Curncy";
    case AssetClass::Index: return "Index";
    case AssetClass::Rate: return "Rate";
  }
  return "Unknown";
}

bool ParseHistoryRange(const std::string& s, HistoryRange* out) {
  if (s == "1D") { *out = HistoryRange::OneDay; return true; }
  if (s == "1M") { *out = HistoryRange::OneMonth; return true; }
  if (s == "1Y") { *out = HistoryRange::OneYear; return true; }
  return false;
}

namespace {

Instrument Eq(const char* t, const char* name, double px, const char* sector,
              double cap, double pe, double dy, double beta, const char* desc) {
  Instrument i;
  i.ticker = t;
  i.name = name;
  i.asset_class = AssetClass::Equity;
  i.region = "Americas";
  i.currency = "USD";
  i.base_price = px;
  i.volatility = 0.00045;
  i.decimals = 2;
  i.spread_bps = 3;
  i.fundamentals.sector = sector;
  i.fundamentals.market_cap_bn = cap;
  i.fundamentals.pe = pe;
  i.fundamentals.dividend_yield = dy;
  i.fundamentals.eps = pe > 0 ? px / pe : 0;
  i.fundamentals.beta = beta;
  i.fundamentals.description = desc;
  return i;
}

Instrument Fx(const char* t, const char* name, double px, int decimals) {
  Instrument i;
  i.ticker = t;
  i.name = name;
  i.asset_class = AssetClass::Fx;
  i.region = "Global";
  i.currency = std::string(t).substr(3, 3);
  i.base_price = px;
  i.volatility = 0.00012;
  i.decimals = decimals;
  i.spread_bps = 0.8;
  return i;
}

Instrument Idx(const char* t, const char* name, const char* region,
               const char* ccy, double px) {
  Instrument i;
  i.ticker = t;
  i.name = name;
  i.asset_class = AssetClass::Index;
  i.region = region;
  i.currency = ccy;
  i.base_price = px;
  i.volatility = 0.00018;
  i.decimals = 2;
  i.spread_bps = 1;
  return i;
}

Instrument Rate(const char* t, const char* name, const char* region,
                const char* ccy, double yield) {
  Instrument i;
  i.ticker = t;
  i.name = name;
  i.asset_class = AssetClass::Rate;
  i.region = region;
  i.currency = ccy;
  i.base_price = yield;
  i.volatility = 0.004;  // absolute, in percentage points per tick
  i.decimals = 3;
  i.spread_bps = 5;
  return i;
}

std::vector<Instrument> Build() {
  return {
      Eq("AAPL US Equity", "Apricot Computer Inc", 228.40, "Technology", 3450, 34.1, 0.44, 1.21,
         "Designs consumer electronics, personal computers and a services ecosystem."),
      Eq("MSFT US Equity", "Microstate Software Corp", 431.15, "Technology", 3200, 36.2, 0.72, 0.93,
         "Develops operating systems, productivity software and cloud infrastructure."),
      Eq("AMZN US Equity", "Amazonia Retail Co", 186.70, "Consumer Discretionary", 1950, 44.8, 0.0, 1.15,
         "Operates an online marketplace, logistics network and cloud computing arm."),
      Eq("GOOGL US Equity", "Googol Holdings Inc", 165.30, "Communication Services", 2040, 23.5, 0.48, 1.04,
         "Provides search, advertising, video and cloud services."),
      Eq("NVDA US Equity", "Nvision Graphics Corp", 121.80, "Technology", 2990, 55.3, 0.03, 1.68,
         "Designs graphics and accelerated computing processors."),
      Eq("META US Equity", "Metaverse Social Inc", 512.60, "Communication Services", 1300, 27.9, 0.39, 1.22,
         "Operates social networking and messaging applications."),
      Eq("JPM US Equity", "JP Morrow Banking Corp", 211.25, "Financials", 600, 11.8, 2.27, 1.08,
         "Global universal bank with investment banking, markets and consumer lending."),
      Eq("XOM US Equity", "Exxo Mobility Energy", 117.90, "Energy", 470, 13.6, 3.22, 0.88,
         "Integrated oil and gas exploration, refining and chemicals company."),
      Eq("TSLA US Equity", "Teslar Motors Inc", 238.50, "Consumer Discretionary", 760, 62.4, 0.0, 2.05,
         "Manufactures electric vehicles and energy storage systems."),
      Eq("KO US Equity", "Coca Cola-esque Co", 70.12, "Consumer Staples", 302, 27.1, 2.77, 0.59,
         "Produces and distributes non-alcoholic beverages worldwide."),

      Fx("EURUSD Curncy", "Euro / US Dollar", 1.1085, 4),
      Fx("USDJPY Curncy", "US Dollar / Japanese Yen", 143.62, 2),
      Fx("GBPUSD Curncy", "British Pound / US Dollar", 1.3120, 4),
      Fx("AUDUSD Curncy", "Australian Dollar / US Dollar", 0.6775, 4),
      Fx("USDCHF Curncy", "US Dollar / Swiss Franc", 0.8465, 4),
      Fx("USDCAD Curncy", "US Dollar / Canadian Dollar", 1.3540, 4),

      Idx("SPX Index", "US Large Cap 500", "Americas", "USD", 5702.55),
      Idx("INDU Index", "US Industrial 30", "Americas", "USD", 42063.36),
      Idx("CCMP Index", "US Tech Composite", "Americas", "USD", 17948.32),
      Idx("SPTSX Index", "Canada Composite", "Americas", "CAD", 23956.40),
      Idx("MEXBOL Index", "Mexico Bolsa", "Americas", "MXN", 52640.10),
      Idx("IBOV Index", "Brazil Bovespa", "Americas", "BRL", 131065.00),
      Idx("UKX Index", "UK 100", "EMEA", "GBP", 8320.76),
      Idx("DAX Index", "Germany 40", "EMEA", "EUR", 18846.00),
      Idx("CAC Index", "France 40", "EMEA", "EUR", 7500.26),
      Idx("SX5E Index", "Euro Area 50", "EMEA", "EUR", 4871.55),
      Idx("IBEX Index", "Spain 35", "EMEA", "EUR", 11746.50),
      Idx("NKY Index", "Japan 225", "APAC", "JPY", 37723.91),
      Idx("HSI Index", "Hong Kong Hang", "APAC", "HKD", 19129.10),
      Idx("SHCOMP Index", "Shanghai Composite", "APAC", "CNY", 2896.31),
      Idx("AS51 Index", "Australia 200", "APAC", "AUD", 8200.40),
      Idx("KOSPI Index", "Korea Composite", "APAC", "KRW", 2593.37),

      Rate("USGG2YR Index", "US Treasury 2Y Yield", "Americas", "USD", 3.554),
      Rate("USGG10YR Index", "US Treasury 10Y Yield", "Americas", "USD", 3.731),
      Rate("GDBR10 Index", "Germany Bund 10Y Yield", "EMEA", "EUR", 2.182),
  };
}

}  // namespace

const std::vector<Instrument>& DefaultInstruments() {
  static const std::vector<Instrument> kInstruments = Build();
  return kInstruments;
}

}  // namespace mockterm::md
