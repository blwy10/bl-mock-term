// mtsrv_chatengine.h                                                 -*-C++-*-
#ifndef INCLUDED_MTSRV_CHATENGINE
#define INCLUDED_MTSRV_CHATENGINE

//@PURPOSE: Simulate counterparties replying to instant-bloomberg-style chat.
//
//@CLASSES:
//  mtsrv::ChatMessage:  one chat line
//  mtsrv::ChatEngine:   delivers outgoing lines and schedules canned replies

#include "mtsrv_security.h"
#include "mtsrv_tickcache.h"

#include <mtcore_element.h>
#include <mtcore_mutex.h>
#include <mtcore_random.h>

#include <list>
#include <string>
#include <vector>

namespace mtsrv {

struct ChatMessage {
    Int64       id;
    Int64       timestamp;
    std::string from;
    std::string to;
    std::string text;

    ChatMessage();

    void toElement(mtcore::Element *element, const std::string& viewer) const;
        // 'direction' is "out" when 'from == viewer', otherwise "in".
};

class ChatEngine {
    struct Pending {
        Int64       d_dueMs;
        std::string d_user;
        std::string d_counterparty;
        std::string d_prompt;
    };
    typedef std::list<Pending> PendingList;

    const SecurityMaster *d_master_p;
    const TickCache      *d_cache_p;
    mtcore::Mutex         d_mutex;
    mtcore::Random        d_rng;
    PendingList           d_pending;
    Int64                 d_nextId;
    Uint64                d_sent;
    Uint64                d_replies;

  private:
    std::string composeReply(const std::string& prompt);

  public:
    ChatEngine(const SecurityMaster *master,
               const TickCache      *cache,
               Uint64                seed);

    ChatMessage send(const std::string& from,
                     const std::string& to,
                     const std::string& text,
                     Int64              nowMs);
        // Record an outgoing message and schedule a reply from 'to' in
        // 0.7 - 2.2 seconds.

    void poll(std::vector<ChatMessage> *result, Int64 nowMs);
        // Append every reply that has become due.

    size_t pendingCount();
    Uint64 sentCount() const;
    Uint64 replyCount() const;
};

}  // close namespace mtsrv

#endif
