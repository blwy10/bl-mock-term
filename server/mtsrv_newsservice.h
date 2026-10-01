// mtsrv_newsservice.h                                                -*-C++-*-
#ifndef INCLUDED_MTSRV_NEWSSERVICE
#define INCLUDED_MTSRV_NEWSSERVICE

//@PURPOSE: Provide "//mt/news": headline search and streaming.
//
//@CLASSES:
//  mtsrv::NewsService:  "Search" requests + headline subscriptions
//
//@DESCRIPTION: Subscription topic "HEADLINES" receives every story; any other
// topic is treated as a search query and receives matching stories only.

#include "mtsrv_newsengine.h"
#include "mtsrv_service.h"
#include "mtsrv_subscriptiontable.h"

namespace mtsrv {

class NewsService : public Service {
    NewsEngine       *d_engine_p;   // held, not owned
    SubscriptionTable d_table;
    volatile LONGLONG d_published;

  public:
    explicit NewsService(NewsEngine *engine);

    void publish(const Headline& headline);
        // Deliver 'headline' to every matching subscription.

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
