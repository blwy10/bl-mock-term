// mtcomm_conflator.h                                                 -*-C++-*-
#ifndef INCLUDED_MTCOMM_CONFLATOR
#define INCLUDED_MTCOMM_CONFLATOR

//@PURPOSE: Coalesce bursts of field updates per downstream subscription.
//
//@CLASSES:
//  mtcomm::ConflatedUpdate:  one merged update ready to send
//  mtcomm::Conflator:        pending merged deltas keyed by subscription
//
//@DESCRIPTION: A client that logs on with 'conflateMs > 0' receives at most
// one update per subscription per interval.  Each update is the field-wise
// merge of everything that arrived in the interval; 'count' records how many
// upstream updates it represents so the engine can set 'e_CONFLATED'.

#include <mtcore_element.h>
#include <mtcore_mutex.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace mtcomm {

using mtcore::Int64;
using mtcore::Uint64;

typedef std::pair<Uint64, Uint64> DownstreamKey;   // (client id, client cid)

struct ConflatedUpdate {
    DownstreamKey   key;
    std::string     service;
    std::string     topic;
    mtcore::Element fields;
    int             count;
    bool            stale;

    ConflatedUpdate();
};

class Conflator {
    struct Pending {
        std::string     d_service;
        std::string     d_topic;
        mtcore::Element d_fields;
        int             d_count;
        Int64           d_firstMs;
    };
    typedef std::map<DownstreamKey, Pending *> PendingMap;

    mutable mtcore::Mutex d_mutex;
    PendingMap            d_pending;   // owned
    Uint64                d_updatesIn;
    Uint64                d_updatesOut;

  private:
    // NOT IMPLEMENTED
    Conflator(const Conflator&);
    Conflator& operator=(const Conflator&);

  public:
    Conflator();
    ~Conflator();

    void add(const DownstreamKey&   key,
             const std::string&     service,
             const std::string&     topic,
             const mtcore::Element& fields,
             Int64                  nowMs);

    void drain(std::vector<ConflatedUpdate> *result,
               Int64                         nowMs,
               const std::map<Uint64, int>&  intervalByClient);
        // Move every pending update whose interval has elapsed into
        // 'result'.  Clients missing from 'intervalByClient' flush at once.

    bool remove(const DownstreamKey& key);
    int removeClient(Uint64 clientId);

    int pendingCount() const;
    Uint64 updatesIn() const;
    Uint64 updatesOut() const;
};

}  // close namespace mtcomm

#endif
