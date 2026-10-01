// mtsrv_mktdataservice.h                                             -*-C++-*-
#ifndef INCLUDED_MTSRV_MKTDATASERVICE
#define INCLUDED_MTSRV_MKTDATASERVICE

//@PURPOSE: Provide "//mt/mktdata": real-time subscriptions and snapshots.
//
//@CLASSES:
//  mtsrv::MktdataService:  streams ticker-plant updates to subscribers
//
//@DESCRIPTION: Topic = security (e.g. "AAPL US Equity"); body.fields = array
// of field mnemonics (default: all real-time fields).  On subscribe the
// service replies 'e_SUBSCRIPTION_STARTED' then an 'e_INITIAL_PAINT' image
// from the last-value cache; every subsequent tick produces one
// 'e_SUBSCRIPTION_DATA' per subscriber.  Operation "Snapshot" returns the
// current image of a list of securities.

#include "mtsrv_service.h"
#include "mtsrv_subscriptiontable.h"
#include "mtsrv_tickerplant.h"

namespace mtsrv {

class MktdataService : public Service, public TickListener {
    TickerPlant      *d_plant_p;     // held, not owned
    SubscriptionTable d_table;
    volatile LONGLONG d_updatesSent;
    volatile LONGLONG d_ticksSeen;

  private:
    void buildUpdate(mtcore::Message     *message,
                     const Subscription&  subscription,
                     const Security&      security,
                     const Tick&          tick,
                     mtcore::Uint32       flags) const;

  public:
    explicit MktdataService(TickerPlant *plant);
    virtual ~MktdataService();

    // Service
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

    // TickListener
    virtual void onTick(const Tick& tick);

    const SubscriptionTable& table() const;
};

}  // close namespace mtsrv

#endif
