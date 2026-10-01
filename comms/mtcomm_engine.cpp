// mtcomm_engine.cpp                                                  -*-C++-*-
#include "mtcomm_engine.h"

#include <mtcore_log.h>
#include <mtcore_stringutil.h>
#include <mtcore_time.h>

namespace mtcomm {

using mtcore::Connection;
using mtcore::ConnectionHandle;
using mtcore::Element;
using mtcore::LockGuard;
using mtcore::Message;
using mtcore::MessageFlags;
using mtcore::MessageType;
using mtcore::StringUtil;
using mtcore::TimeUtil;
using mtcore::Uint32;

namespace {

const char k_COMMS_NAME[]    = "mtcomm";
const char k_MSG_SERVICE[]   = "//mt/msg";
const char k_COMMS_VERSION[] = "1.0";

}  // close unnamed namespace

const char CommsEngine::k_ADMIN_SERVICE[] = "//mtcomm/admin";

CommsEngine::CommsEngine(const CommsConfig& config)
: d_config(config)
, d_acceptRunnable(this, &CommsEngine::acceptLoop)
, d_timerRunnable(this, &CommsEngine::timerLoop)
, d_acceptThread("acceptor")
, d_timerThread("timer")
, d_stopEvent(mtcore::Event::e_MANUAL_RESET)
, d_running(false)
, d_startTimeMs(0)
, d_nextClientId(0)
, d_updatesIn(0)
, d_updatesOut(0)
, d_requestsRouted(0)
, d_cacheHits(0)
, d_recaps(0)
, d_upstreamOutages(0)
, d_totalClients(0)
, d_serverRestarts(0)
{
    mtclient::SessionOptions options;
    options.host                = config.upstreamHost;
    options.port                = config.upstreamPort;
    options.user                = "MTCOMM";
    options.application         = k_COMMS_NAME;
    options.autoReconnect       = true;
    options.reconnectMinMs      = config.reconnectMinMs;
    options.reconnectMaxMs      = config.reconnectMaxMs;
    options.heartbeatIntervalMs = config.heartbeatMs;
    d_upstream_p.reset(new mtclient::Session(options, this));
}

CommsEngine::~CommsEngine()
{
    stop();
}

int CommsEngine::spawnServer()
{
    std::string exe = d_config.serverExe;
    if (exe.empty()) {
        exe = mtcore::ChildProcess::moduleDirectory() + "mtserver.exe";
    }
    std::string logFile = mtcore::ChildProcess::moduleDirectory()
                        + "mtserver.log";
    std::string cmd = mtcore::ChildProcess::quote(exe)
                    + StringUtil::format(" --port %d", d_config.upstreamPort)
                    + " --log-file " + mtcore::ChildProcess::quote(logFile);
    d_server_p.reset(new mtcore::ChildProcess());
    return d_server_p->start(cmd, "", true);
}

int CommsEngine::start()
{
    if (d_running) {
        return 0;
    }
    if (d_listener.listen(d_config.listenAddress.c_str(), d_config.port)
                                                                      != 0) {
        MT_LOG_ERROR("mtcomm: cannot listen on %s:%d",
                     d_config.listenAddress.c_str(),
                     d_config.port);
        return -1;
    }
    if (d_config.spawnServer && spawnServer() != 0) {
        d_listener.close();
        return -1;
    }
    d_startTimeMs = TimeUtil::nowMillis();
    d_running     = true;
    d_stopEvent.reset();
    d_upstream_p->start();
    d_acceptThread.start(&d_acceptRunnable);
    d_timerThread.start(&d_timerRunnable);
    MT_LOG_INFO("%s %s listening on %s:%d, upstream %s:%d",
                k_COMMS_NAME,
                k_COMMS_VERSION,
                d_config.listenAddress.c_str(),
                d_listener.port(),
                d_config.upstreamHost.c_str(),
                d_config.upstreamPort);
    return 0;
}

void CommsEngine::stop()
{
    if (!d_running) {
        return;
    }
    d_running = false;
    d_listener.close();
    d_acceptThread.join();
    d_stopEvent.set();
    d_timerThread.join();
    d_upstream_p->stop();

    std::vector<LocalClientHandle> clients;
    {
        LockGuard guard(&d_clientMutex);
        for (ClientMap::iterator it = d_clientsByConnection.begin();
             it != d_clientsByConnection.end();
             ++it) {
            clients.push_back(it->second);
        }
    }
    for (size_t i = 0; i < clients.size(); ++i) {
        clients[i]->close();
        clients[i]->connection()->join();
    }
    reapClosed();
    {
        LockGuard guard(&d_clientMutex);
        d_clientsByConnection.clear();
        d_clientsById.clear();
    }
    if (d_server_p.get()) {
        d_server_p->terminate();
    }
    MT_LOG_INFO("mtcomm stopped");
}

void CommsEngine::acceptLoop()
{
    while (d_running) {
        mtcore::TcpSocket *socket = d_listener.accept();
        if (socket == NULL) {
            if (d_running) {
                mtcore::Thread::sleep(100);
            }
            continue;
        }
        socket->setNoDelay(true);
        Connection *connection = new Connection(socket, this, "local");
        ConnectionHandle guard(connection);
        LocalClientHandle client(
                     new LocalClient(connection,
                                     (Uint64)d_nextClientId.increment()));
        {
            LockGuard lock(&d_clientMutex);
            d_clientsByConnection[connection->id()] = client;
            d_clientsById[client->clientId()]       = client;
        }
        ::InterlockedIncrement64(&d_totalClients);
        MT_LOG_INFO("local client %I64u connected from %s",
                    client->clientId(),
                    socket->peer().c_str());
        if (connection->start() != 0) {
            LockGuard lock(&d_clientMutex);
            d_clientsByConnection.erase(connection->id());
            d_clientsById.erase(client->clientId());
        }
    }
}

void CommsEngine::timerLoop()
{
    Int64 lastStats = TimeUtil::nowMillis();
    Int64 lastCheck = lastStats;
    while (!d_stopEvent.wait(5)) {
        Int64 now = TimeUtil::nowMillis();
        flushConflated(now);
        if (now - lastCheck >= 1000) {
            superviseServer();
            checkClients(now);
            reapClosed();
            lastCheck = now;
        }
        if (d_config.statsIntervalMs > 0
         && now - lastStats >= d_config.statsIntervalMs) {
            logStats();
            lastStats = now;
        }
    }
}

void CommsEngine::superviseServer()
{
    if (d_server_p.get() == NULL || d_server_p->isRunning()) {
        return;
    }
    MT_LOG_WARN("spawned mtserver exited (code %d); restarting",
                d_server_p->wait(0));
    ::InterlockedIncrement64(&d_serverRestarts);
    spawnServer();
}

void CommsEngine::flushConflated(Int64 nowMs)
{
    if (d_conflator.pendingCount() == 0) {
        return;
    }
    std::map<Uint64, int> intervals;
    {
        LockGuard guard(&d_clientMutex);
        for (ClientMap::iterator it = d_clientsById.begin();
             it != d_clientsById.end();
             ++it) {
            intervals[it->first] = it->second->conflateMs();
        }
    }
    std::vector<ConflatedUpdate> due;
    d_conflator.drain(&due, nowMs, intervals);
    for (size_t i = 0; i < due.size(); ++i) {
        LocalClientHandle client = findClientById(due[i].key.first);
        if (client.isNull()) {
            continue;
        }
        Message out(MessageType::e_SUBSCRIPTION_DATA, due[i].key.second);
        out.setService(due[i].service);
        out.setTopic(due[i].topic);
        out.body() = due[i].fields;
        if (due[i].count > 1) {
            out.addFlags(MessageFlags::e_CONFLATED);
        }
        client->send(out);
        client->countUpdate();
        ::InterlockedIncrement64(&d_updatesOut);
    }
}

void CommsEngine::checkClients(Int64 nowMs)
{
    if (d_config.clientTimeoutMs <= 0) {
        return;
    }
    std::vector<LocalClientHandle> idle;
    {
        LockGuard guard(&d_clientMutex);
        for (ClientMap::iterator it = d_clientsById.begin();
             it != d_clientsById.end();
             ++it) {
            Connection *c = it->second->connection();
            if (nowMs - c->lastReceiveMillis() > d_config.clientTimeoutMs) {
                idle.push_back(it->second);
            }
        }
    }
    for (size_t i = 0; i < idle.size(); ++i) {
        MT_LOG_WARN("local client %I64u idle, disconnecting",
                    idle[i]->clientId());
        idle[i]->close();
    }
}

void CommsEngine::reapClosed()
{
    ClosedList closed;
    {
        LockGuard guard(&d_clientMutex);
        closed.swap(d_closed);
    }
    for (ClosedList::iterator it = closed.begin(); it != closed.end(); ++it) {
        (*it)->join();
    }
}

void CommsEngine::logStats()
{
    MT_LOG_INFO("stats: clients=%d topics=%d cache=%d upstream=%s "
                "in=%I64d out=%I64d conflated=%I64u requests=%I64d",
                clientCount(),
                topicCount(),
                d_cache.size(),
                mtclient::SessionState::toString(d_upstream_p->state()),
                (Int64)d_updatesIn,
                (Int64)d_updatesOut,
                d_conflator.updatesIn() - d_conflator.updatesOut(),
                (Int64)d_requestsRouted);
}

LocalClientHandle CommsEngine::findClient(Connection *connection) const
{
    LockGuard guard(&d_clientMutex);
    ClientMap::const_iterator it = d_clientsByConnection.find(connection->id());
    return it == d_clientsByConnection.end() ? LocalClientHandle()
                                             : it->second;
}

LocalClientHandle CommsEngine::findClientById(Uint64 clientId) const
{
    LockGuard guard(&d_clientMutex);
    ClientMap::const_iterator it = d_clientsById.find(clientId);
    return it == d_clientsById.end() ? LocalClientHandle() : it->second;
}

void CommsEngine::broadcast(const Message& message)
{
    std::vector<LocalClientHandle> clients;
    {
        LockGuard guard(&d_clientMutex);
        for (ClientMap::iterator it = d_clientsById.begin();
             it != d_clientsById.end();
             ++it) {
            if (it->second->isLoggedOn()) {
                clients.push_back(it->second);
            }
        }
    }
    for (size_t i = 0; i < clients.size(); ++i) {
        clients[i]->send(message);
    }
}

void CommsEngine::onMessage(Connection *connection, const Message& message)
{
    LocalClientHandle client = findClient(connection);
    if (client.isNull()) {
        return;
    }
    switch (message.type()) {
      case MessageType::e_LOGON:
        handleLogon(client.get(), message);
        return;
      case MessageType::e_HEARTBEAT:
        client->send(Message(MessageType::e_HEARTBEAT_ACK,
                             message.correlationId()));
        return;
      case MessageType::e_HEARTBEAT_ACK:
        return;
      case MessageType::e_LOGOFF:
        client->close();
        return;
      default:
        break;
    }
    if (!client->isLoggedOn()) {
        reject(client.get(), message, "NOT_LOGGED_ON", "logon required");
        return;
    }
    switch (message.type()) {
      case MessageType::e_REQUEST:
        handleRequest(client.get(), message);
        break;
      case MessageType::e_SUBSCRIBE:
        handleSubscribe(client.get(), message);
        break;
      case MessageType::e_UNSUBSCRIBE:
        handleUnsubscribe(client.get(), message);
        break;
      default:
        MT_LOG_WARN("local client %I64u: unexpected %s",
                    client->clientId(),
                    message.describe().c_str());
        break;
    }
}

void CommsEngine::onDisconnect(Connection *connection)
{
    LocalClientHandle client;
    {
        LockGuard guard(&d_clientMutex);
        ClientMap::iterator it = d_clientsByConnection.find(connection->id());
        if (it == d_clientsByConnection.end()) {
            return;
        }
        client = it->second;
        d_clientsByConnection.erase(it);
        d_clientsById.erase(client->clientId());
        d_closed.push_back(ConnectionHandle(connection));
    }
    std::vector<RemovedTopic> removed;
    {
        LockGuard guard(&d_routeMutex);
        d_mux.detachClient(&removed, client->clientId());
        RequestRouteMap::iterator it = d_requestRoutes.begin();
        while (it != d_requestRoutes.end()) {
            if (it->second.first == client->clientId()) {
                d_requestRoutes.erase(it++);
            }
            else {
                ++it;
            }
        }
        for (size_t i = 0; i < removed.size(); ++i) {
            d_upstream_p->unsubscribe(removed[i].service,
                                      removed[i].upstreamCid);
            d_cache.remove(removed[i].key);
        }
    }
    d_conflator.removeClient(client->clientId());
    MT_LOG_INFO("local client %I64u disconnected (%d upstream topics "
                "released)",
                client->clientId(),
                (int)removed.size());
}

void CommsEngine::handleLogon(LocalClient *client, const Message& logon)
{
    int conflateMs = (int)logon.body().getChildInt64("conflateMs",
                                                     d_config.defaultConflateMs);
    if (conflateMs < 0) {
        conflateMs = 0;
    }
    if (conflateMs > d_config.maxConflateMs) {
        conflateMs = d_config.maxConflateMs;
    }
    client->logon(logon.body().getChildString("user", "ME"),
                  logon.body().getChildString("application", "unknown"),
                  conflateMs,
                  TimeUtil::nowMillis());

    Message ack(MessageType::e_LOGON_ACK, logon.correlationId());
    Element& body = ack.body();
    body.setChildInt64("sessionId", (Int64)client->clientId());
    body.setChildString("server", k_COMMS_NAME);
    body.setChildString("version", k_COMMS_VERSION);
    body.setChildDatetime("serverTime", TimeUtil::nowMillis());
    body.setChildInt64("conflateMs", conflateMs);
    body.setChildString("upstream",
                 mtclient::SessionState::toString(d_upstream_p->state()));
    {
        LockGuard guard(&d_routeMutex);
        const Element *services = d_upstreamAck.body().getChild("services");
        if (services) {
            *body.addChild("services") = *services;
        }
    }
    client->send(ack);
    MT_LOG_INFO("local client %I64u logged on: user=%s application=%s "
                "conflateMs=%d",
                client->clientId(),
                client->user().c_str(),
                client->application().c_str(),
                conflateMs);
}

void CommsEngine::reject(LocalClient        *client,
                         const Message&      request,
                         const char         *category,
                         const std::string&  text)
{
    MessageType::Value type = request.type() == MessageType::e_SUBSCRIBE
                            ? MessageType::e_SUBSCRIPTION_FAILURE
                            : MessageType::e_REQUEST_FAILURE;
    Message failure(type, request.correlationId());
    failure.setService(request.service());
    failure.setOperation(request.operation());
    failure.setTopic(request.topic());
    Element *reason = failure.body().addChild("reason");
    reason->setChildInt64("code", 100);
    reason->setChildString("category", category);
    reason->setChildString("message", text);
    client->send(failure);
}

void CommsEngine::handleRequest(LocalClient *client, const Message& request)
{
    client->countRequest();
    if (request.service() == k_ADMIN_SERVICE) {
        handleAdmin(client, request);
        return;
    }
    Uint64 upstreamCid = d_upstream_p->nextCorrelationId();
    {
        LockGuard guard(&d_routeMutex);
        d_requestRoutes[upstreamCid] =
                     DownstreamKey(client->clientId(), request.correlationId());
    }
    Message forward(request);
    forward.setCorrelationId(upstreamCid);
    if (request.service() == k_MSG_SERVICE) {
        forward.body().setChildString("from", client->user());
    }
    if (d_upstream_p->sendRequest(forward) != 0) {
        LockGuard guard(&d_routeMutex);
        d_requestRoutes.erase(upstreamCid);
        reject(client, request, "INTERNAL", "duplicate correlation id");
        return;
    }
    ::InterlockedIncrement64(&d_requestsRouted);
}

void CommsEngine::handleAdmin(LocalClient *client, const Message& request)
{
    if (request.operation() != "Status") {
        reject(client, request, "BAD_OPERATION",
               "unknown operation '" + request.operation() + "'");
        return;
    }
    Message response(MessageType::e_RESPONSE, request.correlationId());
    response.setService(request.service());
    response.setOperation(request.operation());
    buildStatus(&response.body());
    client->send(response);
}

void CommsEngine::handleSubscribe(LocalClient *client, const Message& msg)
{
    if (msg.topic().empty()) {
        reject(client, msg, "BAD_TOPIC", "empty topic");
        return;
    }
    DownstreamKey key(client->clientId(), msg.correlationId());
    LockGuard guard(&d_routeMutex);
    bool created = false;
    Uint64 upstreamCid = d_upstream_p->nextCorrelationId();
    UpstreamTopic *topic = d_mux.attach(&created, key, msg, upstreamCid);
    if (topic == NULL) {
        reject(client, msg, "DUPLICATE_CORRELATION_ID",
               "correlation id already subscribed");
        return;
    }
    if (created) {
        d_upstream_p->subscribe(topic->subscribe);
        return;
    }
    if (!topic->started) {
        return;   // the upstream STARTED will be fanned out
    }
    Message started(MessageType::e_SUBSCRIPTION_STARTED, msg.correlationId());
    started.setService(topic->service);
    started.setTopic(topic->topic);
    client->send(started);

    Element image;
    bool stale = false;
    if (topic->painted && d_cache.get(&image, &stale, topic->key)) {
        Message paint(MessageType::e_SUBSCRIPTION_DATA, msg.correlationId());
        paint.setService(topic->service);
        paint.setTopic(topic->topic);
        paint.setFlags(MessageFlags::e_INITIAL_PAINT);
        if (stale) {
            paint.addFlags(MessageFlags::e_STALE);
        }
        paint.body() = image;
        client->send(paint);
        ::InterlockedIncrement64(&d_cacheHits);
    }
}

void CommsEngine::handleUnsubscribe(LocalClient *client, const Message& msg)
{
    DownstreamKey key(client->clientId(), msg.correlationId());
    d_conflator.remove(key);
    LockGuard guard(&d_routeMutex);
    RemovedTopic removed;
    if (d_mux.detach(&removed, key)) {
        d_upstream_p->unsubscribe(removed.service, removed.upstreamCid);
        d_cache.remove(removed.key);
    }
}

void CommsEngine::processMessage(mtclient::Session *, const Message& message)
{
    switch (message.type()) {
      case MessageType::e_RESPONSE:
      case MessageType::e_PARTIAL_RESPONSE:
      case MessageType::e_REQUEST_FAILURE:
        routeResponse(message);
        break;
      case MessageType::e_SUBSCRIPTION_STARTED:
        routeSubscriptionStarted(message);
        break;
      case MessageType::e_SUBSCRIPTION_DATA:
        routeSubscriptionData(message);
        break;
      case MessageType::e_SUBSCRIPTION_FAILURE:
      case MessageType::e_SUBSCRIPTION_TERMINATED:
        routeSubscriptionEnd(message);
        break;
      case MessageType::e_SERVICE_STATUS:
      case MessageType::e_SESSION_STATUS:
        broadcast(message);
        break;
      default:
        MT_LOG_DEBUG("upstream: ignoring %s", message.describe().c_str());
        break;
    }
}

void CommsEngine::routeResponse(const Message& message)
{
    DownstreamKey route;
    {
        LockGuard guard(&d_routeMutex);
        RequestRouteMap::iterator it =
                               d_requestRoutes.find(message.correlationId());
        if (it == d_requestRoutes.end()) {
            return;
        }
        route = it->second;
        if (message.type() != MessageType::e_PARTIAL_RESPONSE) {
            d_requestRoutes.erase(it);
        }
    }
    LocalClientHandle client = findClientById(route.first);
    if (client.isNull()) {
        return;
    }
    Message out(message);
    out.setCorrelationId(route.second);
    client->send(out);
}

void CommsEngine::routeSubscriptionStarted(const Message& message)
{
    std::vector<DownstreamKey> subscribers;
    std::string service;
    std::string topicName;
    {
        LockGuard guard(&d_routeMutex);
        UpstreamTopic *topic = d_mux.findByUpstream(message.correlationId());
        if (topic == NULL || topic->started) {
            return;   // unknown, or a restore after reconnect
        }
        topic->started = true;
        subscribers.assign(topic->subscribers.begin(),
                           topic->subscribers.end());
        service   = topic->service;
        topicName = topic->topic;
    }
    for (size_t i = 0; i < subscribers.size(); ++i) {
        LocalClientHandle client = findClientById(subscribers[i].first);
        if (client.isNull()) {
            continue;
        }
        Message out(message);
        out.setCorrelationId(subscribers[i].second);
        client->send(out);
    }
}

void CommsEngine::routeSubscriptionData(const Message& message)
{
    ::InterlockedIncrement64(&d_updatesIn);
    Int64 now = TimeUtil::nowMillis();
    bool paint = message.hasFlag(MessageFlags::e_INITIAL_PAINT)
              || message.hasFlag(MessageFlags::e_RECAP);
    std::vector<DownstreamKey> subscribers;
    std::string key;
    std::string service;
    std::string topicName;
    bool conflatable = false;
    Uint32 flags = message.flags();
    {
        LockGuard guard(&d_routeMutex);
        UpstreamTopic *topic = d_mux.findByUpstream(message.correlationId());
        if (topic == NULL) {
            return;
        }
        ++topic->updates;
        if (paint) {
            if (topic->painted) {
                flags = (flags & ~(Uint32)MessageFlags::e_INITIAL_PAINT)
                      | MessageFlags::e_RECAP;
                ::InterlockedIncrement64(&d_recaps);
            }
            topic->painted = true;
            topic->started = true;
        }
        d_cache.update(topic->key, message.body(), paint, now);
        subscribers.assign(topic->subscribers.begin(),
                           topic->subscribers.end());
        key         = topic->key;
        service     = topic->service;
        topicName   = topic->topic;
        conflatable = topic->subscribe.body().hasChild("fields");
    }
    for (size_t i = 0; i < subscribers.size(); ++i) {
        LocalClientHandle client = findClientById(subscribers[i].first);
        if (client.isNull()) {
            continue;
        }
        if (!paint && conflatable && client->conflateMs() > 0) {
            d_conflator.add(subscribers[i], service, topicName,
                            message.body(), now);
            continue;
        }
        if (paint) {
            d_conflator.remove(subscribers[i]);
        }
        Message out(message);
        out.setCorrelationId(subscribers[i].second);
        out.setFlags(flags);
        client->send(out);
        client->countUpdate();
        ::InterlockedIncrement64(&d_updatesOut);
    }
}

void CommsEngine::routeSubscriptionEnd(const Message& message)
{
    std::vector<DownstreamKey> subscribers;
    std::string key;
    {
        LockGuard guard(&d_routeMutex);
        if (!d_mux.removeTopic(&subscribers, &key, message.correlationId())) {
            return;
        }
        d_cache.remove(key);
    }
    for (size_t i = 0; i < subscribers.size(); ++i) {
        d_conflator.remove(subscribers[i]);
        LocalClientHandle client = findClientById(subscribers[i].first);
        if (client.isNull()) {
            continue;
        }
        Message out(message);
        out.setCorrelationId(subscribers[i].second);
        client->send(out);
    }
}

void CommsEngine::processSessionState(mtclient::Session            *,
                                      mtclient::SessionState::Value state,
                                      const Message                *logonAck)
{
    Message status(MessageType::e_SESSION_STATUS, 0);
    status.setService(k_ADMIN_SERVICE);
    if (state == mtclient::SessionState::e_UP) {
        {
            LockGuard guard(&d_routeMutex);
            if (logonAck) {
                d_upstreamAck = *logonAck;
            }
        }
        status.body().setChildString("status", "UPSTREAM_UP");
        status.body().setChildString("server",
                         logonAck ? logonAck->body().getChildString("server")
                                  : std::string());
        MT_LOG_INFO("upstream %s:%d is UP",
                    d_config.upstreamHost.c_str(),
                    d_config.upstreamPort);
    }
    else {
        ::InterlockedIncrement64(&d_upstreamOutages);
        d_cache.markAllStale();
        status.body().setChildString("status", "UPSTREAM_DOWN");
        MT_LOG_WARN("upstream %s:%d is DOWN; %d cached topics now stale",
                    d_config.upstreamHost.c_str(),
                    d_config.upstreamPort,
                    d_cache.size());
    }
    status.body().setChildDatetime("time", TimeUtil::nowMillis());
    broadcast(status);
}

void CommsEngine::buildStatus(Element *status) const
{
    Int64 now = TimeUtil::nowMillis();
    status->setChildString("server", k_COMMS_NAME);
    status->setChildString("version", k_COMMS_VERSION);
    status->setChildInt64("uptimeMs", now - d_startTimeMs);
    status->setChildInt64("port", port());

    Element *up = status->addChild("upstream");
    up->setChildString("host", d_config.upstreamHost);
    up->setChildInt64("port", d_config.upstreamPort);
    up->setChildString("state",
                 mtclient::SessionState::toString(d_upstream_p->state()));
    up->setChildInt64("subscriptions", d_upstream_p->numSubscriptions());
    up->setChildInt64("pendingRequests", d_upstream_p->numPendingRequests());
    up->setChildInt64("reconnects", d_upstream_p->reconnectCount());
    up->setChildInt64("outages", (Int64)d_upstreamOutages);
    up->setChildInt64("serverRestarts", (Int64)d_serverRestarts);

    Element *counters = status->addChild("counters");
    counters->setChildInt64("updatesIn", (Int64)d_updatesIn);
    counters->setChildInt64("updatesOut", (Int64)d_updatesOut);
    counters->setChildInt64("conflatedIn", (Int64)d_conflator.updatesIn());
    counters->setChildInt64("conflatedOut", (Int64)d_conflator.updatesOut());
    counters->setChildInt64("requestsRouted", (Int64)d_requestsRouted);
    counters->setChildInt64("cacheHits", (Int64)d_cacheHits);
    counters->setChildInt64("recaps", (Int64)d_recaps);
    counters->setChildInt64("totalClients", (Int64)d_totalClients);

    status->setChildInt64("cacheEntries", d_cache.size());
    status->setChildInt64("staleEntries", d_cache.staleCount());

    Element *clients = status->addChild("clients");
    clients->setArray(true);
    {
        LockGuard guard(&d_clientMutex);
        for (ClientMap::const_iterator it = d_clientsById.begin();
             it != d_clientsById.end();
             ++it) {
            Element *c = clients->appendItem();
            c->setChildInt64("id", (Int64)it->first);
            c->setChildString("user", it->second->user());
            c->setChildString("application", it->second->application());
            c->setChildInt64("conflateMs", it->second->conflateMs());
            c->setChildInt64("updatesSent", it->second->updatesSent());
            c->setChildInt64("requests", it->second->requests());
        }
    }

    Element *topics = status->addChild("topics");
    topics->setArray(true);
    {
        LockGuard guard(&d_routeMutex);
        std::vector<const UpstreamTopic *> list;
        d_mux.topics(&list);
        for (size_t i = 0; i < list.size(); ++i) {
            Element *t = topics->appendItem();
            t->setChildString("service", list[i]->service);
            t->setChildString("topic", list[i]->topic);
            t->setChildInt64("subscribers", (Int64)list[i]->subscribers.size());
            t->setChildInt64("updates", (Int64)list[i]->updates);
        }
    }
}

int CommsEngine::port() const
{
    return d_listener.port();
}

int CommsEngine::clientCount() const
{
    LockGuard guard(&d_clientMutex);
    return (int)d_clientsById.size();
}

int CommsEngine::topicCount() const
{
    LockGuard guard(&d_routeMutex);
    return d_mux.topicCount();
}

bool CommsEngine::isUpstreamUp() const
{
    return d_upstream_p->isUp();
}

mtclient::Session *CommsEngine::upstream() const
{
    return d_upstream_p.get();
}

const LastValueCache& CommsEngine::cache() const
{
    return d_cache;
}

const Conflator& CommsEngine::conflator() const
{
    return d_conflator;
}

}  // close namespace mtcomm
