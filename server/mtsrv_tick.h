// mtsrv_tick.h                                                       -*-C++-*-
#ifndef INCLUDED_MTSRV_TICK
#define INCLUDED_MTSRV_TICK

//@PURPOSE: Provide normalized market-data value types.
//
//@CLASSES:
//  mtsrv::Tick:          normalized top-of-book + session statistics
//  mtsrv::Bar:           OHLCV bar
//  mtsrv::HistoryRange:  enumeration of history windows
//  mtsrv::PriceUtil:     rounding helpers

#include <mtcore_platform.h>

#include <string>

namespace mtsrv {

using mtcore::Int64;
using mtcore::Uint64;

struct Tick {
    std::string ticker;
    std::string feed;
    double      bid;
    double      ask;
    double      last;
    double      open;
    double      high;
    double      low;
    double      prevClose;
    double      change;
    double      changePct;
    Int64       volume;
    Int64       lastSize;
    Int64       timestamp;   // epoch ms
    Uint64      sequence;    // ticker-plant sequence number

    Tick();
};

struct Bar {
    Int64  timestamp;
    double open;
    double high;
    double low;
    double close;
    Int64  volume;

    Bar();
};

struct HistoryRange {
    enum Value {
        e_ONE_DAY   = 0,
        e_ONE_MONTH = 1,
        e_ONE_YEAR  = 2
    };

    static const char *toString(Value value);
    static int fromString(Value *result, const std::string& text);
        // Accepts "1D", "1M", "1Y".  Return 0 on success.
};

struct PriceUtil {
    static double round(double value, int decimals);
    static double tickSize(int decimals);
};

}  // close namespace mtsrv

#endif
