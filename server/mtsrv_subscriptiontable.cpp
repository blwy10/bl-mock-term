// mtsrv_subscriptiontable.cpp                                        -*-C++-*-
#include "mtsrv_subscriptiontable.h"

namespace mtsrv {

using mtcore::LockGuard;

Subscription::Subscription()
: correlationId(0)
, createdMs(0)
, updatesSent(0)
{
}

SubscriptionTable::SubscriptionTable()
{
}

int SubscriptionTable::add(const Subscription& subscription)
{
    Key key(subscription.session->sessionId(), subscription.correlationId);
    LockGuard guard(&d_mutex);
    if (d_subscriptions.find(key) != d_subscriptions.end()) {
        return -1;
    }
    d_subscriptions[key] = subscription;
    d_byTopic[subscription.topic].insert(key);
    return 0;
}

bool SubscriptionTable::remove(Subscription *removed,
                               Uint64        sessionId,
                               Uint64        correlationId)
{
    Key key(sessionId, correlationId);
    LockGuard guard(&d_mutex);
    SubscriptionMap::iterator it = d_subscriptions.find(key);
    if (it == d_subscriptions.end()) {
        return false;
    }
    TopicIndex::iterator t = d_byTopic.find(it->second.topic);
    if (t != d_byTopic.end()) {
        t->second.erase(key);
        if (t->second.empty()) {
            d_byTopic.erase(t);
        }
    }
    if (removed) {
        *removed = it->second;
    }
    d_subscriptions.erase(it);
    return true;
}

int SubscriptionTable::removeSession(std::vector<Subscription> *removed,
                                     Uint64                     sessionId)
{
    LockGuard guard(&d_mutex);
    int count = 0;
    SubscriptionMap::iterator it =
                            d_subscriptions.lower_bound(Key(sessionId, 0));
    while (it != d_subscriptions.end() && it->first.first == sessionId) {
        TopicIndex::iterator t = d_byTopic.find(it->second.topic);
        if (t != d_byTopic.end()) {
            t->second.erase(it->first);
            if (t->second.empty()) {
                d_byTopic.erase(t);
            }
        }
        if (removed) {
            removed->push_back(it->second);
        }
        d_subscriptions.erase(it++);
        ++count;
    }
    return count;
}

void SubscriptionTable::subscribersOf(std::vector<Subscription> *result,
                                      const std::string&         topic) const
{
    LockGuard guard(&d_mutex);
    TopicIndex::const_iterator t = d_byTopic.find(topic);
    if (t == d_byTopic.end()) {
        return;
    }
    for (std::set<Key>::const_iterator k = t->second.begin();
         k != t->second.end();
         ++k) {
        SubscriptionMap::const_iterator it = d_subscriptions.find(*k);
        if (it != d_subscriptions.end()) {
            result->push_back(it->second);
        }
    }
}

void SubscriptionTable::allSubscriptions(
                                    std::vector<Subscription> *result) const
{
    LockGuard guard(&d_mutex);
    for (SubscriptionMap::const_iterator it = d_subscriptions.begin();
         it != d_subscriptions.end();
         ++it) {
        result->push_back(it->second);
    }
}

void SubscriptionTable::topics(std::vector<std::string> *result) const
{
    LockGuard guard(&d_mutex);
    for (TopicIndex::const_iterator it = d_byTopic.begin();
         it != d_byTopic.end();
         ++it) {
        result->push_back(it->first);
    }
}

void SubscriptionTable::countUpdate(const Key& key)
{
    LockGuard guard(&d_mutex);
    SubscriptionMap::iterator it = d_subscriptions.find(key);
    if (it != d_subscriptions.end()) {
        ++it->second.updatesSent;
    }
}

int SubscriptionTable::size() const
{
    LockGuard guard(&d_mutex);
    return (int)d_subscriptions.size();
}

int SubscriptionTable::topicCount() const
{
    LockGuard guard(&d_mutex);
    return (int)d_byTopic.size();
}

bool SubscriptionTable::hasTopic(const std::string& topic) const
{
    LockGuard guard(&d_mutex);
    return d_byTopic.find(topic) != d_byTopic.end();
}

}  // close namespace mtsrv
