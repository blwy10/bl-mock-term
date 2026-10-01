// mtcomm_localclient.cpp                                             -*-C++-*-
#include "mtcomm_localclient.h"

namespace mtcomm {

LocalClient::LocalClient(mtcore::Connection *connection, Uint64 clientId)
: d_connection(connection)
, d_clientId(clientId)
, d_conflateMs(0)
, d_loggedOn(false)
, d_logonTimeMs(0)
, d_updatesSent(0)
, d_requests(0)
{
}

void LocalClient::logon(const std::string& user,
                        const std::string& application,
                        int                conflateMs,
                        Int64              nowMs)
{
    d_user        = user;
    d_application = application;
    d_conflateMs  = conflateMs;
    d_logonTimeMs = nowMs;
    d_loggedOn    = true;
}

bool LocalClient::send(const mtcore::Message& message)
{
    return d_connection->send(message);
}

void LocalClient::close()
{
    d_connection->close();
}

void LocalClient::countUpdate()
{
    ::InterlockedIncrement64(&d_updatesSent);
}

void LocalClient::countRequest()
{
    ::InterlockedIncrement64(&d_requests);
}

Uint64 LocalClient::clientId() const
{
    return d_clientId;
}

const std::string& LocalClient::user() const
{
    return d_user;
}

const std::string& LocalClient::application() const
{
    return d_application;
}

int LocalClient::conflateMs() const
{
    return d_conflateMs;
}

bool LocalClient::isLoggedOn() const
{
    return d_loggedOn;
}

bool LocalClient::isOpen() const
{
    return d_connection->isOpen();
}

Int64 LocalClient::logonTimeMs() const
{
    return d_logonTimeMs;
}

Int64 LocalClient::updatesSent() const
{
    return d_updatesSent;
}

Int64 LocalClient::requests() const
{
    return d_requests;
}

mtcore::Connection *LocalClient::connection() const
{
    return d_connection.get();
}

}  // close namespace mtcomm
