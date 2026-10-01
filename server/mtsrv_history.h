// mtsrv_history.h                                                    -*-C++-*-
#ifndef INCLUDED_MTSRV_HISTORY
#define INCLUDED_MTSRV_HISTORY

//@PURPOSE: Synthesize historical bars and merge in captured intraday bars.
//
//@CLASSES:
//  mtsrv::HistoryGenerator:  deterministic bar synthesis per ticker/range

#include "mtsrv_security.h"
#include "mtsrv_tick.h"

#include <vector>

namespace mtsrv {

struct HistoryGenerator {
    static void generate(std::vector<Bar>   *result,
                         const Security&     security,
                         HistoryRange::Value range,
                         double              lastPrice,
                         Int64               nowMs);
        // Replace 'result' with a deterministic series (seeded from the
        // ticker and range) that ends at 'lastPrice'.
        // 1D = 390 one-minute bars, 1M = 154 bars, 1Y = 252 daily bars.

    static int overlay(std::vector<Bar>        *bars,
                       const std::vector<Bar>&  live);
        // Replace the tail of 'bars' with the captured 'live' bars so that
        // the most recent part of an intraday chart reflects real ticks.
        // Return the number of bars replaced.

    static int barCount(HistoryRange::Value range);
};

}  // close namespace mtsrv

#endif
