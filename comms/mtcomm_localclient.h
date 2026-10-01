// mtcomm_localclient.h                                               -*-C++-*-
#ifndef INCLUDED_MTCOMM_LOCALCLIENT
#define INCLUDED_MTCOMM_LOCALCLIENT

//@PURPOSE: Represent one local application connected to mtcomm.
//
//@CLASSES:
//  mtcomm::LocalClient:  connection, identity and delivery preferences

#include <mtcore_connection.h>
#include <mtcore_refcount.h>

#include <string>

namespace mtcomm {

using mtcore::Int64;
using mtcore::Uint64;

class LocalClient : public mtcore::RefCounted {
    mtcore::ConnectionHandle d_connection;
    Uint64                   d_clientId;
    std::string              d_user;
    std::string              d_application;
    int                      d_conflateMs;
    volatile bool            d_loggedOn;
    Int64                    d_logonTimeMs;
    volatile LONGLONG        d_updatesSent;
    volatile LONGLONG        d_requests;

  public:
    LocalClient(mtcore::Connection *connection, Uint64 clientId);

    void logon(const std::string& user,
               const std::string& application,
               int                conflateMs,
               Int64              nowMs);
    bool send(const mtcore::Message& message);
    void close();
    void countUpdate();
    void countRequest();

    Uint64 clientId() const;
    const std::string& user() const;
    const std::string& application() const;
    int conflateMs() const;
    bool isLoggedOn() const;
    bool isOpen() const;
    Int64 logonTimeMs() const;
    Int64 updatesSent() const;
    Int64 requests() const;
    mtcore::Connection *connection() const;
};

typedef mtcore::Handle<LocalClient> LocalClientHandle;

}  // close namespace mtcomm

#endif
