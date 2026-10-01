// mtcomm_topicmux.cpp                                                -*-C++-*-
#include "mtcomm_topicmux.h"

#include <mtcore_stringutil.h>
#include <mtcore_time.h>

#include <algorithm>

namespace mtcomm {

using mtcore::Element;
using mtcore::Message;
using mtcore::StringUtil;

UpstreamTopic::UpstreamTopic()
: upstreamCid(0)
, started(false)
, painted(false)
, createdMs(0)
, updates(0)
{
}

TopicMux::TopicMux()
{
}

TopicMux::~TopicMux()
{
    for (KeyIndex::iterator it = d_byKey.begin(); it != d_byKey.end(); ++it) {
        delete it->second;
    }
    d_byKey.clear();
}

std::string TopicMux::makeKey(const Message& subscribe)
{
    std::vector<std::string> fields;
    const Element *list = subscribe.body().getChild("fields");
    if (list) {
        for (size_t i = 0; i < list->numChildren(); ++i) {
            fields.push_back(StringUtil::toUpper(list->childAt(i)->getString()));
        }
    }
    std::sort(fields.begin(), fields.end());
    fields.erase(std::unique(fields.begin(), fields.end()), fields.end());
    return subscribe.service() + "|" + subscribe.topic() + "|"
         + StringUtil::join(fields, ",");
}

UpstreamTopic *TopicMux::attach(bool                 *created,
                                const DownstreamKey&  downstream,
                                const Message&        subscribe,
                                Uint64                upstreamCid)
{
    *created = false;
    if (d_byDownstream.find(downstream) != d_byDownstream.end()) {
        return NULL;
    }
    std::string key = makeKey(subscribe);
    UpstreamTopic *topic = NULL;
    KeyIndex::iterator it = d_byKey.find(key);
    if (it == d_byKey.end()) {
        topic              = new UpstreamTopic();
        topic->key         = key;
        topic->service     = subscribe.service();
        topic->topic       = subscribe.topic();
        topic->upstreamCid = upstreamCid;
        topic->subscribe   = subscribe;
        topic->subscribe.setCorrelationId(upstreamCid);
        topic->createdMs   = mtcore::TimeUtil::nowMillis();
        d_byKey[key]                = topic;
        d_byUpstreamCid[upstreamCid] = topic;
        *created = true;
    }
    else {
        topic = it->second;
    }
    topic->subscribers.insert(downstream);
    d_byDownstream[downstream] = topic;
    return topic;
}

void TopicMux::erase(UpstreamTopic *topic)
{
    for (std::set<DownstreamKey>::const_iterator s = topic->subscribers.begin();
         s != topic->subscribers.end();
         ++s) {
        d_byDownstream.erase(*s);
    }
    d_byUpstreamCid.erase(topic->upstreamCid);
    d_byKey.erase(topic->key);
    delete topic;
}

bool TopicMux::detach(RemovedTopic *removed, const DownstreamKey& downstream)
{
    ClientIndex::iterator it = d_byDownstream.find(downstream);
    if (it == d_byDownstream.end()) {
        return false;
    }
    UpstreamTopic *topic = it->second;
    d_byDownstream.erase(it);
    topic->subscribers.erase(downstream);
    if (!topic->subscribers.empty()) {
        return false;
    }
    removed->key         = topic->key;
    removed->service     = topic->service;
    removed->upstreamCid = topic->upstreamCid;
    erase(topic);
    return true;
}

void TopicMux::detachClient(std::vector<RemovedTopic> *removed,
                            Uint64                     clientId)
{
    std::vector<DownstreamKey> keys;
    for (ClientIndex::iterator it =
                         d_byDownstream.lower_bound(DownstreamKey(clientId, 0));
         it != d_byDownstream.end() && it->first.first == clientId;
         ++it) {
        keys.push_back(it->first);
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        RemovedTopic r;
        if (detach(&r, keys[i])) {
            removed->push_back(r);
        }
    }
}

bool TopicMux::removeTopic(std::vector<DownstreamKey> *subscribers,
                           std::string                *key,
                           Uint64                      upstreamCid)
{
    CidIndex::iterator it = d_byUpstreamCid.find(upstreamCid);
    if (it == d_byUpstreamCid.end()) {
        return false;
    }
    UpstreamTopic *topic = it->second;
    subscribers->assign(topic->subscribers.begin(), topic->subscribers.end());
    *key = topic->key;
    erase(topic);
    return true;
}

UpstreamTopic *TopicMux::findByUpstream(Uint64 upstreamCid) const
{
    CidIndex::const_iterator it = d_byUpstreamCid.find(upstreamCid);
    return it == d_byUpstreamCid.end() ? NULL : it->second;
}

UpstreamTopic *TopicMux::findByDownstream(const DownstreamKey& downstream) const
{
    ClientIndex::const_iterator it = d_byDownstream.find(downstream);
    return it == d_byDownstream.end() ? NULL : it->second;
}

void TopicMux::topics(std::vector<const UpstreamTopic *> *result) const
{
    for (KeyIndex::const_iterator it = d_byKey.begin();
         it != d_byKey.end();
         ++it) {
        result->push_back(it->second);
    }
}

void TopicMux::markAllUnstarted()
{
    for (KeyIndex::iterator it = d_byKey.begin(); it != d_byKey.end(); ++it) {
        it->second->started = false;
    }
}

int TopicMux::topicCount() const
{
    return (int)d_byKey.size();
}

int TopicMux::subscriberCount() const
{
    return (int)d_byDownstream.size();
}

}  // close namespace mtcomm
