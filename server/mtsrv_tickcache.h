// mtsrv_tickcache.h                                                  -*-C++-*-
#ifndef INCLUDED_MTSRV_TICKCACHE
#define INCLUDED_MTSRV_TICKCACHE

//@PURPOSE: Normalize feed packets into ticks and keep the last value.
//
//@CLASSES:
//  mtsrv::TickCache:  last-value cache + normalizer, thread-safe
//
//@DESCRIPTION: 'apply' merges a raw 'FeedPacket' into the cached 'Tick' for
// its ticker (updating bid/ask, last, high/low, volume, change) and stamps
// it with the next ticker-plant sequence number.

#include "mtsrv_feedhandler.h"
#include "mtsrv_security.h"
#include "mtsrv_tick.h"

#include <mtcore_mutex.h>

#include <map>
#include <string>
#include <vector>

namespace mtsrv {

class TickCache {
    typedef std::map<std::string, Tick> TickMap;

    mutable mtcore::Mutex d_mutex;
    TickMap               d_ticks;
    Uint64                d_sequence;

  public:
    TickCache();

    void initialize(const Security& security,
                    double          openPrice,
                    Int64           openingVolume,
                    Int64           nowMs);
        // Seed the session state for 'security'.

    bool apply(Tick            *result,
               const FeedPacket& packet,
               const Security&   security);
        // Return 'false' if the ticker was never initialized.

    bool get(Tick *result, const std::string& ticker) const;
    void snapshot(std::vector<Tick> *result) const;
    Uint64 lastSequence() const;
    int size() const;
};

}  // close namespace mtsrv

#endif
