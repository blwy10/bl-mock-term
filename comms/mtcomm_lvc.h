// mtcomm_lvc.h                                                       -*-C++-*-
#ifndef INCLUDED_MTCOMM_LVC
#define INCLUDED_MTCOMM_LVC

//@PURPOSE: Provide the local last-value cache for multiplexed topics.
//
//@CLASSES:
//  mtcomm::LastValueCache:  topic key -> merged field image
//
//@DESCRIPTION: The cache lets a second local subscriber to an already-open
// topic receive an immediate initial paint without another upstream round
// trip, and keeps the last image available (flagged stale) across upstream
// outages.

#include <mtcore_element.h>
#include <mtcore_mutex.h>

#include <map>
#include <string>
#include <vector>

namespace mtcomm {

using mtcore::Int64;
using mtcore::Uint64;

struct CacheEntry {
    mtcore::Element image;
    Int64           updatedMs;
    Uint64          updates;
    bool            stale;

    CacheEntry();
};

class LastValueCache {
    typedef std::map<std::string, CacheEntry *> EntryMap;

    mutable mtcore::Mutex d_mutex;
    EntryMap              d_entries;     // owned
    Uint64                d_totalUpdates;

  private:
    // NOT IMPLEMENTED
    LastValueCache(const LastValueCache&);
    LastValueCache& operator=(const LastValueCache&);

  public:
    LastValueCache();
    ~LastValueCache();

    void update(const std::string&     key,
                const mtcore::Element& fields,
                bool                   replace,
                Int64                  nowMs);
        // Merge 'fields' into the image for 'key' (or replace it), creating
        // the entry if necessary, and clear its stale flag.

    bool get(mtcore::Element *image, const std::string& key) const;
    bool get(mtcore::Element    *image,
             bool               *stale,
             const std::string&  key) const;
    bool remove(const std::string& key);
    void markAllStale();
    int staleCount() const;
    void keys(std::vector<std::string> *result) const;

    int size() const;
    Uint64 totalUpdates() const;
};

}  // close namespace mtcomm

#endif
