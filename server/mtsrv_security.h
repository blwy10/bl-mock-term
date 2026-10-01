// mtsrv_security.h                                                   -*-C++-*-
#ifndef INCLUDED_MTSRV_SECURITY
#define INCLUDED_MTSRV_SECURITY

//@PURPOSE: Provide the security master: static data for every instrument.
//
//@CLASSES:
//  mtsrv::AssetClass:      enumeration of asset classes
//  mtsrv::Fundamentals:    equity fundamentals
//  mtsrv::Security:        static description of one instrument
//  mtsrv::SecurityMaster:  the instrument universe, indexed by ticker

#include <mtcore_platform.h>

#include <map>
#include <string>
#include <vector>

namespace mtsrv {

struct AssetClass {
    enum Value {
        e_EQUITY   = 0,
        e_CURRENCY = 1,
        e_INDEX    = 2,
        e_RATE     = 3
    };

    static const char *toString(Value value);
        // Yellow-key style suffix: "Equity", "Curncy", "Index", "Rate".
};

struct Fundamentals {
    std::string sector;
    double      marketCapBn;
    double      pe;
    double      dividendYield;
    double      eps;
    double      beta;
    std::string description;

    Fundamentals();
};

struct Security {
    std::string       ticker;       // e.g. "AAPL US Equity"
    std::string       name;
    AssetClass::Value assetClass;
    std::string       region;       // "Americas", "EMEA", "APAC", "Global"
    std::string       currency;
    std::string       exchange;     // feed / venue code, e.g. "XNAS"
    double            basePrice;    // reference (previous) close
    double            volatility;   // per-tick
    int               decimals;
    double            spreadBps;
    Fundamentals      fundamentals;

    Security();

    std::string symbol() const;
        // Ticker up to the first space, e.g. "AAPL".
};

class SecurityMaster {
    typedef std::vector<Security>     SecurityList;
    typedef std::map<std::string, int> TickerIndex;

    SecurityList d_securities;
    TickerIndex  d_index;

  public:
    SecurityMaster();

    void loadDefaults();
        // Replace the universe with the built-in 38 instruments.
    int add(const Security& security);
        // Return 0 on success, non-zero if the ticker already exists.

    const Security *find(const std::string& ticker) const;
    int size() const;
    const Security& at(int index) const;

    void search(std::vector<const Security *> *result,
                const std::string&             query,
                int                            maxResults) const;
        // Case-insensitive match on ticker prefix, then name substring.

    void exchanges(std::vector<std::string> *result) const;
    void securitiesOn(std::vector<const Security *> *result,
                      const std::string&             exchange) const;
};

}  // close namespace mtsrv

#endif
