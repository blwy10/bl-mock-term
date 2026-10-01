// mtcore_time.h                                                      -*-C++-*-
#ifndef INCLUDED_MTCORE_TIME
#define INCLUDED_MTCORE_TIME

//@PURPOSE: Provide wall-clock and monotonic time utilities.

#include "mtcore_platform.h"

#include <string>

namespace mtcore {

struct TimeUtil {
    enum {
        k_MILLIS_PER_SECOND = 1000,
        k_MILLIS_PER_MINUTE = 60 * 1000,
        k_MILLIS_PER_HOUR   = 60 * 60 * 1000
    };

    static const Int64 k_MILLIS_PER_DAY;

    static Int64 nowMillis();
        // Milliseconds since 1970-01-01T00:00:00Z.

    static Int64 monotonicMicros();
        // Microseconds from an arbitrary, monotonically increasing origin.

    static std::string formatIso(Int64 epochMillis);
        // "YYYY-MM-DDTHH:MM:SS.mmmZ" (UTC).

    static std::string formatTime(Int64 epochMillis);
        // "HH:MM:SS" (UTC).

    static Int64 floorToMinute(Int64 epochMillis);
};

}  // close namespace mtcore

#endif
