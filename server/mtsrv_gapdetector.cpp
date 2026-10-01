// mtsrv_gapdetector.cpp                                              -*-C++-*-
#include "mtsrv_gapdetector.h"

namespace mtsrv {

GapDetector::GapDetector()
: d_gaps(0)
, d_missing(0)
, d_duplicates(0)
{
}

SequenceCheck::Value GapDetector::check(const std::string& feed,
                                        Uint64             sequence)
{
    ExpectedMap::iterator it = d_expected.find(feed);
    if (it == d_expected.end()) {
        d_expected[feed] = sequence + 1;
        return SequenceCheck::e_OK;
    }
    Uint64 expected = it->second;
    if (sequence == expected) {
        it->second = sequence + 1;
        return SequenceCheck::e_OK;
    }
    if (sequence < expected) {
        ++d_duplicates;
        return SequenceCheck::e_DUPLICATE;
    }
    ++d_gaps;
    d_missing += sequence - expected;
    it->second = sequence + 1;
    return SequenceCheck::e_GAP;
}

void GapDetector::reset(const std::string& feed)
{
    d_expected.erase(feed);
}

Uint64 GapDetector::gapCount() const
{
    return d_gaps;
}

Uint64 GapDetector::missingCount() const
{
    return d_missing;
}

Uint64 GapDetector::duplicateCount() const
{
    return d_duplicates;
}

Uint64 GapDetector::expected(const std::string& feed) const
{
    ExpectedMap::const_iterator it = d_expected.find(feed);
    return it == d_expected.end() ? 0 : it->second;
}

}  // close namespace mtsrv
