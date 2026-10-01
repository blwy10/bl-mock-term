// mtsrv_refdataservice.h                                             -*-C++-*-
#ifndef INCLUDED_MTSRV_REFDATASERVICE
#define INCLUDED_MTSRV_REFDATASERVICE

//@PURPOSE: Provide "//mt/refdata": static data, history and field metadata.
//
//@CLASSES:
//  mtsrv::RefdataService:  request/response reference-data service
//
//@DESCRIPTION: Operations:
//: o "SecurityList":    every security with static data.
//: o "SecuritySearch":  {query, maxResults} -> matches.
//: o "ReferenceData":   {securities[], fields[]} -> securityData[].
//: o "HistoricalData":  {security, range "1D"|"1M"|"1Y"} -> bars[].
//: o "FieldList":       the field dictionary.

#include "mtsrv_service.h"
#include "mtsrv_tickerplant.h"

namespace mtsrv {

class RefdataService : public Service {
    TickerPlant      *d_plant_p;    // held, not owned
    volatile LONGLONG d_requests;

  private:
    void securityList(ClientSession *session, const mtcore::Message& request);
    void securitySearch(ClientSession         *session,
                        const mtcore::Message& request);
    void referenceData(ClientSession *session, const mtcore::Message& request);
    void historicalData(ClientSession         *session,
                        const mtcore::Message& request);
    void fieldList(ClientSession *session, const mtcore::Message& request);

  public:
    explicit RefdataService(TickerPlant *plant);

    static void describeSecurity(mtcore::Element *element,
                                 const Security&  security);

    virtual const char *name() const;
    virtual const char *description() const;
    virtual void handleRequest(ClientSession         *session,
                               const mtcore::Message& request);
    virtual void appendStatistics(mtcore::Element *stats) const;
    virtual void describeOperations(mtcore::Element *operations) const;
};

}  // close namespace mtsrv

#endif
