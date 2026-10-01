// mtsrv_msgservice.h                                                 -*-C++-*-
#ifndef INCLUDED_MTSRV_MSGSERVICE
#define INCLUDED_MTSRV_MSGSERVICE

//@PURPOSE: Provide "//mt/msg": instant-message send and inbox streaming.
//
//@CLASSES:
//  mtsrv::MsgService:  "Send" requests + per-user inbox subscriptions
//
//@DESCRIPTION: Subscribing with topic = user name (e.g. "ME") delivers every
// message addressed to that user.  "Send" {from, to, text} echoes the stored
// outgoing message in the response; replies from simulated counterparties
// arrive later on the sender's inbox subscription.

#include "mtsrv_chatengine.h"
#include "mtsrv_service.h"
#include "mtsrv_subscriptiontable.h"

namespace mtsrv {

class MsgService : public Service {
    ChatEngine       *d_engine_p;   // held, not owned
    SubscriptionTable d_table;
    volatile LONGLONG d_delivered;

  public:
    explicit MsgService(ChatEngine *engine);

    void deliver(const ChatMessage& message);
        // Send 'message' to every inbox subscription for 'message.to'.

    virtual const char *name() const;
    virtual const char *description() const;
    virtual void handleRequest(ClientSession         *session,
                               const mtcore::Message& request);
    virtual void handleSubscribe(ClientSession         *session,
                                 const mtcore::Message& subscribe);
    virtual void handleUnsubscribe(ClientSession *session,
                                   Uint64         correlationId);
    virtual void sessionClosed(ClientSession *session);
    virtual void appendStatistics(mtcore::Element *stats) const;
    virtual void describeOperations(mtcore::Element *operations) const;
};

}  // close namespace mtsrv

#endif
