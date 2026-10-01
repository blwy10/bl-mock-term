// mtsrv_clientsession.cpp                                            -*-C++-*-
#include "mtsrv_clientsession.h"

namespace mtsrv {

ClientSession::ClientSession(mtcore::Connection *connection, Uint64 sessionId)
: d_connection(connection)
, d_sessionId(sessionId)
, d_loggedOn(false)
, d_logonTimeMs(0)
, d_requests(0)
, d_subscriptions(0)
{
}

void ClientSession::logon(const std::string& user,
                          const std::string& application,
                          Int64              nowMs)
{
    d_user        = user;
    d_application = application;
    d_logonTimeMs = nowMs;
    d_loggedOn    = true;
}

bool ClientSession::send(const mtcore::Message& message)
{
    return d_connection->send(message);
}

void ClientSession::close()
{
    d_connection->close();
}

void ClientSession::countRequest()
{
    ::InterlockedIncrement(&d_requests);
}

void ClientSession::adjustSubscriptions(int delta)
{
    ::InterlockedExchangeAdd(&d_subscriptions, delta);
}

Uint64 ClientSession::sessionId() const
{
    return d_sessionId;
}

const std::string& ClientSession::user() const
{
    return d_user;
}

const std::string& ClientSession::application() const
{
    return d_application;
}

bool ClientSession::isLoggedOn() const
{
    return d_loggedOn;
}

bool ClientSession::isOpen() const
{
    return d_connection->isOpen();
}

Int64 ClientSession::logonTimeMs() const
{
    return d_logonTimeMs;
}

int ClientSession::requestCount() const
{
    return (int)d_requests;
}

int ClientSession::subscriptionCount() const
{
    return (int)d_subscriptions;
}

mtcore::Connection *ClientSession::connection() const
{
    return d_connection.get();
}

}  // close namespace mtsrv
