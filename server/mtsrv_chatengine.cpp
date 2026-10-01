// mtsrv_chatengine.cpp                                               -*-C++-*-
#include "mtsrv_chatengine.h"

#include <mtcore_stringutil.h>

#include <stdio.h>

namespace mtsrv {

using mtcore::LockGuard;
using mtcore::StringUtil;

namespace {

const char *k_CANNED[] = {
    "copy that, will revert shortly",
    "seeing decent two-way flow here, nothing crazy",
    "market feels heavy into the close tbh",
    "can work that for you, what size?",
    "noted. desk is a bit thin today",
    "agree, let's touch base after the data",
    "lol yes. anyway, levels look fine from here"
};

}  // close unnamed namespace

ChatMessage::ChatMessage()
: id(0)
, timestamp(0)
{
}

void ChatMessage::toElement(mtcore::Element   *element,
                            const std::string& viewer) const
{
    element->setChildInt64("id", id);
    element->setChildDatetime("time", timestamp);
    element->setChildString("from", from);
    element->setChildString("to", to);
    element->setChildString("text", text);
    element->setChildString("direction", from == viewer ? "out" : "in");
}

ChatEngine::ChatEngine(const SecurityMaster *master,
                       const TickCache      *cache,
                       Uint64                seed)
: d_master_p(master)
, d_cache_p(cache)
, d_rng(seed)
, d_nextId(1)
, d_sent(0)
, d_replies(0)
{
}

ChatMessage ChatEngine::send(const std::string& from,
                             const std::string& to,
                             const std::string& text,
                             Int64              nowMs)
{
    LockGuard guard(&d_mutex);
    Pending pending;
    pending.d_dueMs        = nowMs + d_rng.range(700, 2200);
    pending.d_user         = from;
    pending.d_counterparty = to;
    pending.d_prompt       = text;
    d_pending.push_back(pending);

    ChatMessage m;
    m.id        = d_nextId++;
    m.timestamp = nowMs;
    m.from      = from;
    m.to        = to;
    m.text      = text;
    ++d_sent;
    return m;
}

std::string ChatEngine::composeReply(const std::string& prompt)
{
    std::string upper = StringUtil::toUpper(prompt);
    for (int i = 0; i < d_master_p->size(); ++i) {
        const Security& sec = d_master_p->at(i);
        if (upper.find(sec.symbol()) == std::string::npos) {
            continue;
        }
        Tick tick;
        if (!d_cache_p->get(&tick, sec.ticker)) {
            continue;
        }
        char buf[256];
        _snprintf(buf,
                  sizeof(buf),
                  "%s showing %.*f / %.*f, last %.*f (%+.2f%%). want me to "
                  "work an order?",
                  sec.ticker.c_str(),
                  sec.decimals, tick.bid,
                  sec.decimals, tick.ask,
                  sec.decimals, tick.last,
                  tick.changePct);
        buf[sizeof(buf) - 1] = 0;
        return std::string(buf);
    }
    if (StringUtil::startsWith(upper, "HI")
     || StringUtil::startsWith(upper, "HELLO")
     || upper.find("MORNING") != std::string::npos) {
        return "morning! what can I do for you?";
    }
    return k_CANNED[d_rng.range(0, (int)MTCORE_ARRAY_SIZE(k_CANNED) - 1)];
}

void ChatEngine::poll(std::vector<ChatMessage> *result, Int64 nowMs)
{
    LockGuard guard(&d_mutex);
    PendingList::iterator it = d_pending.begin();
    while (it != d_pending.end()) {
        if (it->d_dueMs > nowMs) {
            ++it;
            continue;
        }
        ChatMessage m;
        m.id        = d_nextId++;
        m.timestamp = nowMs;
        m.from      = it->d_counterparty;
        m.to        = it->d_user;
        m.text      = composeReply(it->d_prompt);
        result->push_back(m);
        ++d_replies;
        it = d_pending.erase(it);
    }
}

size_t ChatEngine::pendingCount()
{
    LockGuard guard(&d_mutex);
    return d_pending.size();
}

Uint64 ChatEngine::sentCount() const
{
    return d_sent;
}

Uint64 ChatEngine::replyCount() const
{
    return d_replies;
}

}  // close namespace mtsrv
