// mtcomm_conflator.cpp                                               -*-C++-*-
#include "mtcomm_conflator.h"

namespace mtcomm {

using mtcore::LockGuard;

ConflatedUpdate::ConflatedUpdate()
: count(0)
, stale(false)
{
}

Conflator::Conflator()
: d_updatesIn(0)
, d_updatesOut(0)
{
}

Conflator::~Conflator()
{
    for (PendingMap::iterator it = d_pending.begin();
         it != d_pending.end();
         ++it) {
        delete it->second;
    }
    d_pending.clear();
}

void Conflator::add(const DownstreamKey&   key,
                    const std::string&     service,
                    const std::string&     topic,
                    const mtcore::Element& fields,
                    Int64                  nowMs)
{
    LockGuard guard(&d_mutex);
    ++d_updatesIn;
    PendingMap::iterator it = d_pending.find(key);
    if (it == d_pending.end()) {
        Pending *p   = new Pending();
        p->d_service = service;
        p->d_topic   = topic;
        p->d_fields  = fields;
        p->d_count   = 1;
        p->d_firstMs = nowMs;
        d_pending[key] = p;
        return;
    }
    it->second->d_fields.merge(fields);
    ++it->second->d_count;
}

void Conflator::drain(std::vector<ConflatedUpdate> *result,
                      Int64                         nowMs,
                      const std::map<Uint64, int>&  intervalByClient)
{
    LockGuard guard(&d_mutex);
    PendingMap::iterator it = d_pending.begin();
    while (it != d_pending.end()) {
        int interval = 0;
        std::map<Uint64, int>::const_iterator c =
                                     intervalByClient.find(it->first.first);
        if (c != intervalByClient.end()) {
            interval = c->second;
        }
        if (nowMs - it->second->d_firstMs < interval) {
            ++it;
            continue;
        }
        ConflatedUpdate update;
        update.key     = it->first;
        update.service = it->second->d_service;
        update.topic   = it->second->d_topic;
        update.fields  = it->second->d_fields;
        update.count   = it->second->d_count;
        result->push_back(update);
        ++d_updatesOut;
        delete it->second;
        d_pending.erase(it++);
    }
}

bool Conflator::remove(const DownstreamKey& key)
{
    LockGuard guard(&d_mutex);
    PendingMap::iterator it = d_pending.find(key);
    if (it == d_pending.end()) {
        return false;
    }
    delete it->second;
    d_pending.erase(it);
    return true;
}

int Conflator::removeClient(Uint64 clientId)
{
    LockGuard guard(&d_mutex);
    int count = 0;
    PendingMap::iterator it = d_pending.lower_bound(DownstreamKey(clientId, 0));
    while (it != d_pending.end() && it->first.first == clientId) {
        delete it->second;
        d_pending.erase(it++);
        ++count;
    }
    return count;
}

int Conflator::pendingCount() const
{
    LockGuard guard(&d_mutex);
    return (int)d_pending.size();
}

Uint64 Conflator::updatesIn() const
{
    LockGuard guard(&d_mutex);
    return d_updatesIn;
}

Uint64 Conflator::updatesOut() const
{
    LockGuard guard(&d_mutex);
    return d_updatesOut;
}

}  // close namespace mtcomm
