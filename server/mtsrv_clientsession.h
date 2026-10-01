// mtsrv_clientsession.h                                              -*-C++-*-
#ifndef INCLUDED_MTSRV_CLIENTSESSION
#define INCLUDED_MTSRV_CLIENTSESSION

//@PURPOSE: Represent one logged-on client connection (usually an mtcomm).
//
//@CLASSES:
//  mtsrv::ClientSession:  identity + connection + per-session statistics

#include <mtcore_connection.h>
#include <mtcore_refcount.h>

#include <string>

namespace mtsrv {

using mtcore::Int64;
using mtcore::Uint64;

class ClientSession : public mtcore::RefCounted {
    mtcore::ConnectionHandle d_connection;
    Uint64                   d_sessionId;
    std::string              d_user;
    std::string              d_application;
    volatile bool            d_loggedOn;
    Int64                    d_logonTimeMs;
    volatile LONG            d_requests;
    volatile LONG            d_subscriptions;

  public:
    ClientSession(mtcore::Connection *connection, Uint64 sessionId);

    void logon(const std::string& user,
               const std::string& application,
               Int64              nowMs);

    bool send(const mtcore::Message& message);
        // Return 'false' if the connection has closed.
    void close();

    void countRequest();
    void adjustSubscriptions(int delta);

    Uint64 sessionId() const;
    const std::string& user() const;
    const std::string& application() const;
    bool isLoggedOn() const;
    bool isOpen() const;
    Int64 logonTimeMs() const;
    int requestCount() const;
    int subscriptionCount() const;
    mtcore::Connection *connection() const;
};

typedef mtcore::Handle<ClientSession> ClientSessionHandle;

}  // close namespace mtsrv

#endif
