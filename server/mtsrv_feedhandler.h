// mtsrv_feedhandler.h                                                -*-C++-*-
#ifndef INCLUDED_MTSRV_FEEDHANDLER
#define INCLUDED_MTSRV_FEEDHANDLER

//@PURPOSE: Simulate one exchange feed producing raw quote/trade packets.
//
//@CLASSES:
//  mtsrv::FeedPacket:   raw, un-normalized exchange message
//  mtsrv::FeedHandler:  random-walk packet generator for one venue
//
//@DESCRIPTION: Each venue (e.g. "XNAS", "FXSPOT") has its own handler with
// its own per-feed sequence number.  A configurable fraction of packets is
// "lost" (the sequence number advances but no packet is emitted) so the
// ticker plant's gap detection has something to detect.

#include "mtsrv_security.h"

#include <mtcore_random.h>

#include <string>
#include <vector>

namespace mtsrv {

using mtcore::Int64;
using mtcore::Uint64;

struct FeedPacket {
    enum Kind {
        e_QUOTE = 'Q',
        e_TRADE = 'T'
    };

    std::string feed;
    Uint64      feedSequence;
    char        kind;
    std::string ticker;
    double      bid;
    double      ask;
    double      price;
    Int64       size;
    Int64       exchangeTimeMs;

    FeedPacket();
};

class FeedHandler {
    struct InstrumentState {
        const Security *d_security_p;
        double          d_last;
    };
    typedef std::vector<InstrumentState> StateList;

    std::string    d_feedId;
    StateList      d_instruments;
    mtcore::Random d_rng;
    Uint64         d_nextSequence;
    double         d_dropProbability;
    Uint64         d_dropped;
    Uint64         d_emitted;

  private:
    // NOT IMPLEMENTED
    FeedHandler(const FeedHandler&);
    FeedHandler& operator=(const FeedHandler&);

  public:
    FeedHandler(const std::string& feedId, Uint64 seed);

    void addSecurity(const Security *security, double openPrice);
    void setDropProbability(double probability);

    void poll(std::vector<FeedPacket> *packets,
              Int64                    nowMs,
              double                   tickProbability);
        // For each instrument, with 'tickProbability', advance its price and
        // append a packet.

    const std::string& feedId() const;
    int numInstruments() const;
    Uint64 nextSequence() const;
    Uint64 droppedPackets() const;
    Uint64 emittedPackets() const;
};

}  // close namespace mtsrv

#endif
