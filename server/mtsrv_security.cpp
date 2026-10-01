// mtsrv_security.cpp                                                 -*-C++-*-
#include "mtsrv_security.h"

#include <mtcore_stringutil.h>

#include <algorithm>
#include <set>

namespace mtsrv {

using mtcore::StringUtil;

const char *AssetClass::toString(Value value)
{
    switch (value) {
      case e_EQUITY:   return "Equity";
      case e_CURRENCY: return "Curncy";
      case e_INDEX:    return "Index";
      case e_RATE:     return "Rate";
    }
    return "Unknown";
}

Fundamentals::Fundamentals()
: marketCapBn(0)
, pe(0)
, dividendYield(0)
, eps(0)
, beta(1)
{
}

Security::Security()
: assetClass(AssetClass::e_EQUITY)
, basePrice(0)
, volatility(0)
, decimals(2)
, spreadBps(2)
{
}

std::string Security::symbol() const
{
    return StringUtil::firstWord(ticker);
}

namespace {

Security makeEquity(const char *ticker,
                    const char *name,
                    const char *exchange,
                    double      price,
                    const char *sector,
                    double      cap,
                    double      pe,
                    double      dvdYield,
                    double      beta,
                    const char *description)
{
    Security s;
    s.ticker                   = ticker;
    s.name                     = name;
    s.assetClass               = AssetClass::e_EQUITY;
    s.region                   = "Americas";
    s.currency                 = "USD";
    s.exchange                 = exchange;
    s.basePrice                = price;
    s.volatility               = 0.00045;
    s.decimals                 = 2;
    s.spreadBps                = 3;
    s.fundamentals.sector        = sector;
    s.fundamentals.marketCapBn   = cap;
    s.fundamentals.pe            = pe;
    s.fundamentals.dividendYield = dvdYield;
    s.fundamentals.eps           = pe > 0 ? price / pe : 0;
    s.fundamentals.beta          = beta;
    s.fundamentals.description   = description;
    return s;
}

Security makeFx(const char *ticker, const char *name, double price, int dp)
{
    Security s;
    s.ticker     = ticker;
    s.name       = name;
    s.assetClass = AssetClass::e_CURRENCY;
    s.region     = "Global";
    s.currency   = std::string(ticker).substr(3, 3);
    s.exchange   = "FXSPOT";
    s.basePrice  = price;
    s.volatility = 0.00012;
    s.decimals   = dp;
    s.spreadBps  = 0.8;
    return s;
}

Security makeIndex(const char *ticker,
                   const char *name,
                   const char *region,
                   const char *ccy,
                   double      price)
{
    Security s;
    s.ticker     = ticker;
    s.name       = name;
    s.assetClass = AssetClass::e_INDEX;
    s.region     = region;
    s.currency   = ccy;
    s.exchange   = std::string("IDX-") + StringUtil::toUpper(
                                              std::string(region).substr(0, 4));
    s.basePrice  = price;
    s.volatility = 0.00018;
    s.decimals   = 2;
    s.spreadBps  = 1;
    return s;
}

Security makeRate(const char *ticker,
                  const char *name,
                  const char *region,
                  const char *ccy,
                  double      yield)
{
    Security s;
    s.ticker     = ticker;
    s.name       = name;
    s.assetClass = AssetClass::e_RATE;
    s.region     = region;
    s.currency   = ccy;
    s.exchange   = "GOVT";
    s.basePrice  = yield;
    s.volatility = 0.004;   // absolute, percentage points per tick
    s.decimals   = 3;
    s.spreadBps  = 5;
    return s;
}

}  // close unnamed namespace

SecurityMaster::SecurityMaster()
{
}

int SecurityMaster::add(const Security& security)
{
    if (d_index.find(security.ticker) != d_index.end()) {
        return -1;
    }
    d_index[security.ticker] = (int)d_securities.size();
    d_securities.push_back(security);
    return 0;
}

const Security *SecurityMaster::find(const std::string& ticker) const
{
    TickerIndex::const_iterator it = d_index.find(ticker);
    return it == d_index.end() ? NULL : &d_securities[it->second];
}

int SecurityMaster::size() const
{
    return (int)d_securities.size();
}

const Security& SecurityMaster::at(int index) const
{
    return d_securities[index];
}

void SecurityMaster::search(std::vector<const Security *> *result,
                            const std::string&             query,
                            int                            maxResults) const
{
    std::string q = StringUtil::toUpper(StringUtil::trim(query));
    std::set<int> taken;
    for (int i = 0; i < size() && (int)result->size() < maxResults; ++i) {
        if (StringUtil::startsWith(StringUtil::toUpper(d_securities[i].ticker),
                                   q)) {
            result->push_back(&d_securities[i]);
            taken.insert(i);
        }
    }
    for (int i = 0; i < size() && (int)result->size() < maxResults; ++i) {
        if (taken.count(i) == 0
         && StringUtil::containsNoCase(d_securities[i].name, q)) {
            result->push_back(&d_securities[i]);
        }
    }
}

void SecurityMaster::exchanges(std::vector<std::string> *result) const
{
    std::set<std::string> seen;
    for (SecurityList::const_iterator it = d_securities.begin();
         it != d_securities.end();
         ++it) {
        if (seen.insert(it->exchange).second) {
            result->push_back(it->exchange);
        }
    }
}

void SecurityMaster::securitiesOn(std::vector<const Security *> *result,
                                  const std::string&             exchange) const
{
    for (SecurityList::const_iterator it = d_securities.begin();
         it != d_securities.end();
         ++it) {
        if (it->exchange == exchange) {
            result->push_back(&*it);
        }
    }
}

void SecurityMaster::loadDefaults()
{
    d_securities.clear();
    d_index.clear();

    add(makeEquity("AAPL US Equity", "Apricot Computer Inc", "XNAS", 228.40,
                   "Technology", 3450, 34.1, 0.44, 1.21,
                   "Designs consumer electronics, personal computers and a "
                   "services ecosystem."));
    add(makeEquity("MSFT US Equity", "Microstate Software Corp", "XNAS",
                   431.15, "Technology", 3200, 36.2, 0.72, 0.93,
                   "Develops operating systems, productivity software and "
                   "cloud infrastructure."));
    add(makeEquity("AMZN US Equity", "Amazonia Retail Co", "XNAS", 186.70,
                   "Consumer Discretionary", 1950, 44.8, 0.0, 1.15,
                   "Operates an online marketplace, logistics network and "
                   "cloud computing arm."));
    add(makeEquity("GOOGL US Equity", "Googol Holdings Inc", "XNAS", 165.30,
                   "Communication Services", 2040, 23.5, 0.48, 1.04,
                   "Provides search, advertising, video and cloud services."));
    add(makeEquity("NVDA US Equity", "Nvision Graphics Corp", "XNAS", 121.80,
                   "Technology", 2990, 55.3, 0.03, 1.68,
                   "Designs graphics and accelerated computing processors."));
    add(makeEquity("META US Equity", "Metaverse Social Inc", "XNAS", 512.60,
                   "Communication Services", 1300, 27.9, 0.39, 1.22,
                   "Operates social networking and messaging applications."));
    add(makeEquity("JPM US Equity", "JP Morrow Banking Corp", "XNYS", 211.25,
                   "Financials", 600, 11.8, 2.27, 1.08,
                   "Global universal bank with investment banking, markets "
                   "and consumer lending."));
    add(makeEquity("XOM US Equity", "Exxo Mobility Energy", "XNYS", 117.90,
                   "Energy", 470, 13.6, 3.22, 0.88,
                   "Integrated oil and gas exploration, refining and "
                   "chemicals company."));
    add(makeEquity("TSLA US Equity", "Teslar Motors Inc", "XNAS", 238.50,
                   "Consumer Discretionary", 760, 62.4, 0.0, 2.05,
                   "Manufactures electric vehicles and energy storage "
                   "systems."));
    add(makeEquity("KO US Equity", "Coca Cola-esque Co", "XNYS", 70.12,
                   "Consumer Staples", 302, 27.1, 2.77, 0.59,
                   "Produces and distributes non-alcoholic beverages "
                   "worldwide."));

    add(makeFx("EURUSD Curncy", "Euro / US Dollar", 1.1085, 4));
    add(makeFx("USDJPY Curncy", "US Dollar / Japanese Yen", 143.62, 2));
    add(makeFx("GBPUSD Curncy", "British Pound / US Dollar", 1.3120, 4));
    add(makeFx("AUDUSD Curncy", "Australian Dollar / US Dollar", 0.6775, 4));
    add(makeFx("USDCHF Curncy", "US Dollar / Swiss Franc", 0.8465, 4));
    add(makeFx("USDCAD Curncy", "US Dollar / Canadian Dollar", 1.3540, 4));

    add(makeIndex("SPX Index", "US Large Cap 500", "Americas", "USD", 5702.55));
    add(makeIndex("INDU Index", "US Industrial 30", "Americas", "USD",
                  42063.36));
    add(makeIndex("CCMP Index", "US Tech Composite", "Americas", "USD",
                  17948.32));
    add(makeIndex("SPTSX Index", "Canada Composite", "Americas", "CAD",
                  23956.40));
    add(makeIndex("MEXBOL Index", "Mexico Bolsa", "Americas", "MXN",
                  52640.10));
    add(makeIndex("IBOV Index", "Brazil Bovespa", "Americas", "BRL",
                  131065.00));
    add(makeIndex("UKX Index", "UK 100", "EMEA", "GBP", 8320.76));
    add(makeIndex("DAX Index", "Germany 40", "EMEA", "EUR", 18846.00));
    add(makeIndex("CAC Index", "France 40", "EMEA", "EUR", 7500.26));
    add(makeIndex("SX5E Index", "Euro Area 50", "EMEA", "EUR", 4871.55));
    add(makeIndex("IBEX Index", "Spain 35", "EMEA", "EUR", 11746.50));
    add(makeIndex("NKY Index", "Japan 225", "APAC", "JPY", 37723.91));
    add(makeIndex("HSI Index", "Hong Kong Hang", "APAC", "HKD", 19129.10));
    add(makeIndex("SHCOMP Index", "Shanghai Composite", "APAC", "CNY",
                  2896.31));
    add(makeIndex("AS51 Index", "Australia 200", "APAC", "AUD", 8200.40));
    add(makeIndex("KOSPI Index", "Korea Composite", "APAC", "KRW", 2593.37));

    add(makeRate("USGG2YR Index", "US Treasury 2Y Yield", "Americas", "USD",
                 3.554));
    add(makeRate("USGG10YR Index", "US Treasury 10Y Yield", "Americas", "USD",
                 3.731));
    add(makeRate("GDBR10 Index", "Germany Bund 10Y Yield", "EMEA", "EUR",
                 2.182));
}

}  // close namespace mtsrv
