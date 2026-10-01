// mtcomm_topicmux.h                                                  -*-C++-*-
#ifndef INCLUDED_MTCOMM_TOPICMUX
#define INCLUDED_MTCOMM_TOPICMUX

//@PURPOSE: Multiplex many local subscriptions onto one upstream subscription.
//
//@CLASSES:
//  mtcomm::UpstreamTopic:  one upstream subscription and its local fan-out
//  mtcomm::RemovedTopic:   description of a topic that lost its last client
//  mtcomm::TopicMux:       indexes by topic key, upstream cid and client key
//
//@DESCRIPTION: Two local subscriptions share an upstream subscription when
// they have the same service, topic and (order-insensitive) field list.
// 'TopicMux' is *not* thread-safe; the engine serialises access.

#include "mtcomm_conflator.h"

#include <mtcore_message.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace mtcomm {

struct UpstreamTopic {
    std::string             key;
    std::string             service;
    std::string             topic;
    Uint64                  upstreamCid;
    mtcore::Message         subscribe;     // as sent upstream
    std::set<DownstreamKey> subscribers;
    bool                    started;
    bool                    painted;
    Int64                   createdMs;
    Uint64                  updates;

    UpstreamTopic();
};

struct RemovedTopic {
    std::string key;
    std::string service;
    Uint64      upstreamCid;
};

class TopicMux {
    typedef std::map<std::string, UpstreamTopic *>   KeyIndex;     // owns
    typedef std::map<Uint64, UpstreamTopic *>        CidIndex;
    typedef std::map<DownstreamKey, UpstreamTopic *> ClientIndex;

    KeyIndex    d_byKey;
    CidIndex    d_byUpstreamCid;
    ClientIndex d_byDownstream;

  private:
    void erase(UpstreamTopic *topic);

    // NOT IMPLEMENTED
    TopicMux(const TopicMux&);
    TopicMux& operator=(const TopicMux&);

  public:
    TopicMux();
    ~TopicMux();

    static std::string makeKey(const mtcore::Message& subscribe);

    UpstreamTopic *attach(bool                  *created,
                          const DownstreamKey&   downstream,
                          const mtcore::Message& subscribe,
                          Uint64                 upstreamCid);
        // Attach 'downstream' to the topic described by 'subscribe',
        // creating it with 'upstreamCid' if needed.  Return 0 if
        // 'downstream' is already attached somewhere.

    bool detach(RemovedTopic *removed, const DownstreamKey& downstream);
        // Return 'true' if this was the topic's last subscriber, in which
        // case the topic is destroyed and described in 'removed'.

    void detachClient(std::vector<RemovedTopic> *removed, Uint64 clientId);

    bool removeTopic(std::vector<DownstreamKey> *subscribers,
                     std::string                *key,
                     Uint64                      upstreamCid);

    UpstreamTopic *findByUpstream(Uint64 upstreamCid) const;
    UpstreamTopic *findByDownstream(const DownstreamKey& downstream) const;

    void topics(std::vector<const UpstreamTopic *> *result) const;
    void markAllUnstarted();
    int topicCount() const;
    int subscriberCount() const;
};

}  // close namespace mtcomm

#endif
