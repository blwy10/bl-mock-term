// mtsrv_gapdetector.h                                                -*-C++-*-
#ifndef INCLUDED_MTSRV_GAPDETECTOR
#define INCLUDED_MTSRV_GAPDETECTOR

//@PURPOSE: Detect gaps and duplicates in per-feed sequence numbers.
//
//@CLASSES:
//  mtsrv::SequenceCheck:  result of checking one sequence number
//  mtsrv::GapDetector:    expected-next-sequence tracker per feed
//
//@DESCRIPTION: Not thread-safe; owned by the ticker plant thread.  Counters
// may be read from other threads for statistics.

#include <mtcore_platform.h>

#include <map>
#include <string>

namespace mtsrv {

using mtcore::Uint64;

struct SequenceCheck {
    enum Value {
        e_OK        = 0,
        e_GAP       = 1,
        e_DUPLICATE = 2
    };
};

class GapDetector {
    typedef std::map<std::string, Uint64> ExpectedMap;

    ExpectedMap     d_expected;
    volatile Uint64 d_gaps;
    volatile Uint64 d_missing;
    volatile Uint64 d_duplicates;

  public:
    GapDetector();

    SequenceCheck::Value check(const std::string& feed, Uint64 sequence);
        // On 'e_GAP', the missing count grows by the size of the gap and the
        // expectation resynchronises to 'sequence + 1'.  'e_DUPLICATE' means
        // 'sequence' is older than expected and should be discarded.

    void reset(const std::string& feed);

    Uint64 gapCount() const;
    Uint64 missingCount() const;
    Uint64 duplicateCount() const;
    Uint64 expected(const std::string& feed) const;
};

}  // close namespace mtsrv

#endif
