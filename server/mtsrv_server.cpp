// mtsrv_server.cpp                                                   -*-C++-*-
#include "mtsrv_server.h"

#include "mtsrv_mktdataservice.h"
#include "mtsrv_msgservice.h"
#include "mtsrv_newsservice.h"
#include "mtsrv_refdataservice.h"

#include <mtcore_log.h>
#include <mtcore_time.h>

namespace mtsrv {

using mtcore::Connection;
using mtcore::ConnectionHandle;
using mtcore::Element;
using mtcore::LockGuard;
using mtcore::Message;
using mtcore::MessageType;
using mtcore::TimeUtil;

namespace {

const char k_SERVER_NAME[]    = "mtserver";
const char k_SERVER_VERSION[] = "1.0";

                            // ==================
                            // class AdminService
                            // ==================

class AdminService : public Service {
    // "//mt/admin": operational status of the server.

    Server *d_server_p;   // held, not owned

  public:
    explicit AdminService(Server *server) : d_server_p(server) {}

    virtual const char *name() const { return "//mt/admin"; }
    virtual const char *description() const
    {
        return "Server status, sessions and service catalogue";
    }

    virtual void handleRequest(ClientSession *session, const Message& request)
    {
        Message reply;
        ServiceUtil::initReply(&reply, MessageType::e_RESPONSE, request);
        if (request.operation() == "Status") {
            d_server_p->buildStatus(&reply.body());
        }
        else if (request.operation() == "Services") {
            d_server_p->listServices(reply.body().addChild("services"));
        }
        else if (request.operation() == "Sessions") {
            std::vector<ClientSessionHandle> all;
            d_server_p->sessions(&all);
            Element *list = reply.body().addChild("sessions");
            list->setArray(true);
            for (size_t i = 0; i < all.size(); ++i) {
                Element *s = list->appendItem();
                s->setChildInt64("sessionId", (mtcore::Int64)all[i]->sessionId());
                s->setChildString("user", all[i]->user());
                s->setChildString("application", all[i]->application());
                s->setChildString("peer", all[i]->connection()->peer());
                s->setChildDatetime("logonTime", all[i]->logonTimeMs());
                s->setChildInt64("requests", all[i]->requestCount());
                s->setChildInt64("subscriptions",
                                 all[i]->subscriptionCount());
                s->setChildInt64("messagesIn",
                                 all[i]->connection()->messagesIn());
                s->setChildInt64("messagesOut",
                                 all[i]->connection()->messagesOut());
            }
        }
        else {
            Service::handleRequest(session, request);
            return;
        }
        session->send(reply);
    }

    virtual void describeOperations(Element *operations) const
    {
        operations->appendItem()->setString("Status");
        operations->appendItem()->setString("Services");
        operations->appendItem()->setString("Sessions");
    }
};

                            // ================
                            // class RequestJob
                            // ================

class RequestJob : public mtcore::Job {
    ClientSessionHandle d_session;
    Service            *d_service_p;
    Message             d_request;

  public:
    RequestJob(ClientSession *session, Service *service, const Message& req)
    : d_session(session)
    , d_service_p(service)
    , d_request(req)
    {
    }

    virtual void execute()
    {
        if (!d_session->isOpen()) {
            return;
        }
        Int64 start = TimeUtil::monotonicMicros();
        d_service_p->handleRequest(d_session.get(), d_request);
        MT_LOG_DEBUG("%s %s cid=%I64u took %I64d us",
                     d_request.service().c_str(),
                     d_request.operation().c_str(),
                     d_request.correlationId(),
                     TimeUtil::monotonicMicros() - start);
    }
};

}  // close unnamed namespace

                                // ------------
                                // class Server
                                // ------------

Server::Server(const ServerConfig& config)
: d_config(config)
, d_mktdata_p(NULL)
, d_newsService_p(NULL)
, d_msgService_p(NULL)
, d_acceptRunnable(this, &Server::acceptLoop)
, d_timerRunnable(this, &Server::timerLoop)
, d_acceptThread("acceptor")
, d_timerThread("timer")
, d_stopEvent(mtcore::Event::e_MANUAL_RESET)
, d_running(false)
, d_startMs(0)
, d_totalSessions(0)
, d_requests(0)
{
    d_master.loadDefaults();

    TickerPlantConfig plantConfig;
    plantConfig.seed            = config.seed;
    plantConfig.intervalMs      = config.tickIntervalMs;
    plantConfig.tickProbability = config.tickProbability;
    plantConfig.dropProbability = config.dropProbability;
    d_plant_p.reset(new TickerPlant(&d_master, plantConfig));

    Uint64 seed = config.seed ? config.seed : mtcore::Random::timeSeed();
    d_news_p.reset(new NewsEngine(&d_master, &d_plant_p->cache(), seed + 7));
    d_news_p->setInterval(config.newsMinIntervalMs, config.newsMaxIntervalMs);
    d_chat_p.reset(new ChatEngine(&d_master, &d_plant_p->cache(), seed + 13));

    d_mktdata_p     = new MktdataService(d_plant_p.get());
    d_newsService_p = new NewsService(d_news_p.get());
    d_msgService_p  = new MsgService(d_chat_p.get());
    addService(d_mktdata_p);
    addService(new RefdataService(d_plant_p.get()));
    addService(d_newsService_p);
    addService(d_msgService_p);
    addService(new AdminService(this));
}

Server::~Server()
{
    stop();
    for (ServiceMap::iterator it = d_services.begin();
         it != d_services.end();
         ++it) {
        delete it->second;
    }
    d_services.clear();
}

void Server::addService(Service *service)
{
    d_services[service->name()] = service;
}

Service *Server::findService(const std::string& name) const
{
    ServiceMap::const_iterator it = d_services.find(name);
    return it == d_services.end() ? NULL : it->second;
}

int Server::start()
{
    if (d_running) {
        return 0;
    }
    d_startMs = TimeUtil::nowMillis();
    d_news_p->backfill(d_config.newsBackfill, d_startMs);
    if (d_listener.listen(d_config.listenAddress.c_str(), d_config.port) != 0) {
        MT_LOG_ERROR("cannot listen on %s:%d: %s",
                     d_config.listenAddress.c_str(),
                     d_config.port,
                     mtcore::SocketUtil::errorText(
                                 mtcore::SocketUtil::lastError()).c_str());
        return -1;
    }
    if (d_pool.start(d_config.workerThreads) != 0) {
        d_listener.close();
        return -2;
    }
    if (d_plant_p->start() != 0) {
        d_pool.stop();
        d_listener.close();
        return -3;
    }
    d_running = true;
    d_stopEvent.reset();
    d_acceptThread.start(&d_acceptRunnable);
    d_timerThread.start(&d_timerRunnable);
    MT_LOG_INFO("%s %s listening on %s:%d (%d services, %d securities)",
                k_SERVER_NAME,
                k_SERVER_VERSION,
                d_config.listenAddress.c_str(),
                d_listener.port(),
                (int)d_services.size(),
                d_master.size());
    return 0;
}

void Server::stop()
{
    if (!d_running) {
        return;
    }
    MT_LOG_INFO("server stopping");
    d_running = false;
    d_listener.close();
    d_acceptThread.join();
    d_stopEvent.set();
    d_timerThread.join();

    std::vector<ClientSessionHandle> open;
    sessions(&open);
    for (size_t i = 0; i < open.size(); ++i) {
        open[i]->close();
    }
    for (size_t i = 0; i < open.size(); ++i) {
        open[i]->connection()->join();
    }
    reapClosed();
    d_pool.stop();
    d_plant_p->stop();
    MT_LOG_INFO("server stopped");
}

void Server::acceptLoop()
{
    while (d_running) {
        mtcore::TcpSocket *socket = d_listener.accept();
        if (socket == NULL) {
            if (d_running) {
                MT_LOG_WARN("accept failed: %s",
                            mtcore::SocketUtil::errorText(
                                 mtcore::SocketUtil::lastError()).c_str());
                mtcore::Thread::sleep(100);
            }
            continue;
        }
        Connection *connection = new Connection(socket, this, "client");
        ConnectionHandle guard(connection);
        ClientSessionHandle session(
                     new ClientSession(connection,
                                       (Uint64)d_nextSessionId.increment()));
        {
            LockGuard lock(&d_sessionMutex);
            d_sessions[connection->id()] = session;
        }
        ::InterlockedIncrement64(&d_totalSessions);
        MT_LOG_INFO("accepted %s as session %I64u",
                    socket->peer().c_str(),
                    session->sessionId());
        if (connection->start() != 0) {
            LockGuard lock(&d_sessionMutex);
            d_sessions.erase(connection->id());
        }
    }
}

void Server::timerLoop()
{
    Int64 lastHeartbeatCheck = 0;
    Int64 lastStats          = TimeUtil::nowMillis();
    while (!d_stopEvent.wait(50)) {
        Int64 now = TimeUtil::nowMillis();

        Headline headline;
        if (d_news_p->poll(&headline, now)) {
            d_newsService_p->publish(headline);
        }
        std::vector<ChatMessage> replies;
        d_chat_p->poll(&replies, now);
        for (size_t i = 0; i < replies.size(); ++i) {
            d_msgService_p->deliver(replies[i]);
        }
        if (now - lastHeartbeatCheck >= 1000) {
            checkHeartbeats(now);
            reapClosed();
            lastHeartbeatCheck = now;
        }
        if (d_config.statsIntervalMs > 0
         && now - lastStats >= d_config.statsIntervalMs) {
            logStatistics();
            lastStats = now;
        }
    }
}

void Server::checkHeartbeats(Int64 nowMs)
{
    if (d_config.heartbeatTimeoutMs <= 0) {
        return;
    }
    std::vector<ClientSessionHandle> open;
    sessions(&open);
    for (size_t i = 0; i < open.size(); ++i) {
        Int64 idle = nowMs - open[i]->connection()->lastReceiveMillis();
        if (idle > d_config.heartbeatTimeoutMs) {
            MT_LOG_WARN("session %I64u idle for %I64d ms, disconnecting",
                        open[i]->sessionId(),
                        idle);
            open[i]->close();
        }
    }
}

void Server::reapClosed()
{
    ConnectionList closed;
    {
        LockGuard lock(&d_sessionMutex);
        closed.swap(d_closed);
    }
    for (ConnectionList::iterator it = closed.begin();
         it != closed.end();
         ++it) {
        (*it)->join();
    }
}

void Server::logStatistics()
{
    MT_LOG_INFO("stats: sessions=%d packets=%I64u gaps=%I64u requests=%I64d",
                sessionCount(),
                d_plant_p->packetsProcessed(),
                d_plant_p->gaps().gapCount(),
                (mtcore::Int64)d_requests);
}

void Server::handleLogon(ClientSession *session, const Message& logon)
{
    std::string user = logon.body().getChildString("user", "ME");
    std::string app  = logon.body().getChildString("application", "unknown");
    session->logon(user, app, TimeUtil::nowMillis());

    Message ack(MessageType::e_LOGON_ACK, logon.correlationId());
    Element& body = ack.body();
    body.setChildInt64("sessionId", (mtcore::Int64)session->sessionId());
    body.setChildString("server", k_SERVER_NAME);
    body.setChildString("version", k_SERVER_VERSION);
    body.setChildDatetime("serverTime", TimeUtil::nowMillis());
    body.setChildInt64("heartbeatTimeoutMs", d_config.heartbeatTimeoutMs);
    listServices(body.addChild("services"));
    session->send(ack);
    MT_LOG_INFO("session %I64u logged on: user=%s application=%s",
                session->sessionId(),
                user.c_str(),
                app.c_str());
}

void Server::onMessage(Connection *connection, const Message& message)
{
    ClientSessionHandle session;
    {
        LockGuard lock(&d_sessionMutex);
        SessionMap::iterator it = d_sessions.find(connection->id());
        if (it != d_sessions.end()) {
            session = it->second;
        }
    }
    if (session.isNull()) {
        return;
    }

    switch (message.type()) {
      case MessageType::e_LOGON:
        handleLogon(session.get(), message);
        return;
      case MessageType::e_LOGOFF:
        session->close();
        return;
      case MessageType::e_HEARTBEAT: {
        Message ack(MessageType::e_HEARTBEAT_ACK, message.correlationId());
        session->send(ack);
      } return;
      case MessageType::e_HEARTBEAT_ACK:
        return;
      default:
        break;
    }

    if (!session->isLoggedOn()) {
        Message status(MessageType::e_SESSION_STATUS, message.correlationId());
        status.body().setChildString("status", "REJECTED");
        ServiceUtil::fillError(status.body().addChild("reason"),
                               ErrorCode::e_NOT_LOGGED_ON,
                               "LOGON required");
        session->send(status);
        return;
    }

    Service *service = findService(message.service());
    switch (message.type()) {
      case MessageType::e_REQUEST: {
        if (service == NULL) {
            ServiceUtil::sendRequestFailure(session.get(),
                                            message,
                                            ErrorCode::e_UNKNOWN_SERVICE,
                                            "unknown service: "
                                                + message.service());
            return;
        }
        session->countRequest();
        ::InterlockedIncrement64(&d_requests);
        d_pool.enqueue(new RequestJob(session.get(), service, message));
      } break;
      case MessageType::e_SUBSCRIBE: {
        if (service == NULL) {
            ServiceUtil::sendSubscriptionFailure(session.get(),
                                                 message,
                                                 ErrorCode::e_UNKNOWN_SERVICE,
                                                 "unknown service: "
                                                     + message.service());
            return;
        }
        service->handleSubscribe(session.get(), message);
      } break;
      case MessageType::e_UNSUBSCRIBE: {
        if (service) {
            service->handleUnsubscribe(session.get(),
                                       message.correlationId());
        }
      } break;
      default:
        MT_LOG_WARN("session %I64u: unexpected %s",
                    session->sessionId(),
                    MessageType::toString(message.type()));
        break;
    }
}

void Server::onDisconnect(Connection *connection)
{
    ClientSessionHandle session;
    {
        LockGuard lock(&d_sessionMutex);
        SessionMap::iterator it = d_sessions.find(connection->id());
        if (it == d_sessions.end()) {
            return;
        }
        session = it->second;
        d_sessions.erase(it);
        d_closed.push_back(ConnectionHandle(connection));
    }
    for (ServiceMap::iterator it = d_services.begin();
         it != d_services.end();
         ++it) {
        it->second->sessionClosed(session.get());
    }
    MT_LOG_INFO("session %I64u disconnected (in=%I64d out=%I64d)",
                session->sessionId(),
                connection->messagesIn(),
                connection->messagesOut());
}

int Server::port() const
{
    return d_listener.port();
}

int Server::sessionCount() const
{
    LockGuard lock(&d_sessionMutex);
    return (int)d_sessions.size();
}

void Server::sessions(std::vector<ClientSessionHandle> *result) const
{
    LockGuard lock(&d_sessionMutex);
    for (SessionMap::const_iterator it = d_sessions.begin();
         it != d_sessions.end();
         ++it) {
        result->push_back(it->second);
    }
}

void Server::listServices(Element *services) const
{
    services->setArray(true);
    for (ServiceMap::const_iterator it = d_services.begin();
         it != d_services.end();
         ++it) {
        Element *s = services->appendItem();
        s->setChildString("name", it->second->name());
        s->setChildString("description", it->second->description());
        Element *ops = s->addChild("operations");
        ops->setArray(true);
        it->second->describeOperations(ops);
    }
}

void Server::buildStatus(Element *status) const
{
    Int64 now = TimeUtil::nowMillis();
    status->setChildString("server", k_SERVER_NAME);
    status->setChildString("version", k_SERVER_VERSION);
    status->setChildDatetime("startTime", d_startMs);
    status->setChildInt64("uptimeMs", now - d_startMs);
    status->setChildInt64("sessions", sessionCount());
    status->setChildInt64("totalSessions", d_totalSessions);
    status->setChildInt64("requests", d_requests);
    status->setChildInt64("pendingJobs",
                          const_cast<mtcore::ThreadPool&>(d_pool).numPending());

    Element *plant = status->addChild("tickerPlant");
    plant->setChildInt64("feeds", d_plant_p->numFeeds());
    plant->setChildInt64("instruments", d_plant_p->cache().size());
    plant->setChildInt64("packets", (Int64)d_plant_p->packetsProcessed());
    plant->setChildInt64("trades", (Int64)d_plant_p->tradesProcessed());
    plant->setChildInt64("quotes", (Int64)d_plant_p->quotesProcessed());
    plant->setChildInt64("sequence", (Int64)d_plant_p->cache().lastSequence());
    plant->setChildInt64("gaps", (Int64)d_plant_p->gaps().gapCount());
    plant->setChildInt64("missing", (Int64)d_plant_p->gaps().missingCount());
    plant->setChildInt64("barSeries", d_plant_p->store().seriesCount());

    Element *svc = status->addChild("services");
    for (ServiceMap::const_iterator it = d_services.begin();
         it != d_services.end();
         ++it) {
        Element *s = svc->addChild(it->first);
        s->setArray(false);
        it->second->appendStatistics(s);
    }
}

TickerPlant& Server::plant()
{
    return *d_plant_p;
}

NewsEngine& Server::news()
{
    return *d_news_p;
}

ChatEngine& Server::chat()
{
    return *d_chat_p;
}

const ServerConfig& Server::config() const
{
    return d_config;
}

}  // close namespace mtsrv
