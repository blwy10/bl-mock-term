// mtsrv_subscriptiontable.h                                          -*-C++-*-
#ifndef INCLUDED_MTSRV_SUBSCRIPTIONTABLE
#define INCLUDED_MTSRV_SUBSCRIPTIONTABLE

//@PURPOSE: Track which sessions subscribe to which topics.
//
//@CLASSES:
//  mtsrv::Subscription:       one (session, correlation id) -> topic entry
//  mtsrv::SubscriptionTable:  thread-safe table indexed by key and topic

#include "mtsrv_clientsession.h"
#include "mtsrv_fields.h"

#include <mtcore_mutex.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mtsrv {

struct Subscription {
    ClientSessionHandle           session;
    Uint64                        correlationId;
    std::string                   topic;
    std::vector<const FieldDef *> fields;
    Int64                         createdMs;
    Uint64                        updatesSent;

    Subscription();
};

class SubscriptionTable {
  public:
    typedef std::pair<Uint64, Uint64> Key;   // (session id, correlation id)

  private:
    typedef std::map<Key, Subscription>         SubscriptionMap;
    typedef std::map<std::string, std::set<Key> > TopicIndex;

    mutable mtcore::Mutex d_mutex;
    SubscriptionMap       d_subscriptions;
    TopicIndex            d_byTopic;

  public:
    SubscriptionTable();

    int add(const Subscription& subscription);
        // Return 0 on success, non-zero if the key already exists.
    bool remove(Subscription *removed, Uint64 sessionId, Uint64 correlationId);
    int removeSession(std::vector<Subscription> *removed, Uint64 sessionId);

    void subscribersOf(std::vector<Subscription> *result,
                       const std::string&         topic) const;
    void allSubscriptions(std::vector<Subscription> *result) const;
    void topics(std::vector<std::string> *result) const;
    void countUpdate(const Key& key);

    int size() const;
    int topicCount() const;
    bool hasTopic(const std::string& topic) const;
};

}  // close namespace mtsrv

#endif
