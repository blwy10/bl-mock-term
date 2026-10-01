// mtsrv_newsengine.h                                                 -*-C++-*-
#ifndef INCLUDED_MTSRV_NEWSENGINE
#define INCLUDED_MTSRV_NEWSENGINE

//@PURPOSE: Generate and archive simulated news headlines.
//
//@CLASSES:
//  mtsrv::Headline:    one story
//  mtsrv::NewsEngine:  template-driven generator with a searchable archive

#include "mtsrv_security.h"
#include "mtsrv_tickcache.h"

#include <mtcore_element.h>
#include <mtcore_mutex.h>
#include <mtcore_random.h>

#include <deque>
#include <string>
#include <vector>

namespace mtsrv {

struct Headline {
    Int64                    id;
    Int64                    timestamp;
    std::string              source;
    std::string              category;
    std::vector<std::string> tickers;
    std::string              headline;
    std::string              body;

    Headline();

    void toElement(mtcore::Element *element) const;
    bool matches(const std::string& query) const;
        // Case-insensitive search over text, source, category and tickers.
        // An empty query matches everything.
};

class NewsEngine {
    typedef std::deque<Headline> Archive;

    const SecurityMaster *d_master_p;
    const TickCache      *d_cache_p;
    mutable mtcore::Mutex d_mutex;
    mtcore::Random        d_rng;
    Archive               d_archive;
    size_t                d_capacity;
    Int64                 d_nextId;
    Int64                 d_nextDueMs;
    int                   d_minIntervalMs;
    int                   d_maxIntervalMs;

  public:
    NewsEngine(const SecurityMaster *master,
               const TickCache      *cache,
               Uint64                seed,
               size_t                capacity = 500);

    void setInterval(int minMs, int maxMs);

    Headline generate(Int64 nowMs);
        // Create, archive and return a new headline.
    bool poll(Headline *result, Int64 nowMs);
        // If a headline is due, generate it into 'result' and return 'true'.
    void backfill(int count, Int64 nowMs);
        // Seed the archive with 'count' stories spread over the last hour.

    void search(std::vector<Headline> *result,
                const std::string&     query,
                size_t                 limit) const;
        // Newest first.
    size_t archiveSize() const;
};

}  // close namespace mtsrv

#endif
