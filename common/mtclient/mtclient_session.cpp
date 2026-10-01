// mtclient_session.cpp                                               -*-C++-*-
#include <mtclient_session.h>

#include <mtcore_log.h>
#include <mtcore_socket.h>
#include <mtcore_time.h>

namespace mtclient {

using mtcore::Connection;
using mtcore::ConnectionHandle;
using mtcore::LockGuard;
using mtcore::Message;
using mtcore::MessageType;
using mtcore::TimeUtil;

const char *SessionState::toString(Value value)
{
    switch (value) {
      case e_STOPPED:    return "STOPPED";
      case e_CONNECTING: return "CONNECTING";
      case e_UP:         return "UP";
      case e_DOWN:       return "DOWN";
    }
    return "UNKNOWN";
}

SessionOptions::SessionOptions()
: host("127.0.0.1")
, port(8194)
, user("ME")
, application("mtclient")
, conflateMs(0)
, autoReconnect(true)
, logonTimeoutMs(5000)
, reconnectMinMs(250)
, reconnectMaxMs(5000)
, heartbeatIntervalMs(5000)
, requestTimeoutMs(15000)
{
}

EventHandler::~EventHandler()
{
}

Session::Session(const SessionOptions& options, EventHandler *handler)
: d_options(options)
, d_handler_p(handler)
, d_state(SessionState::e_STOPPED)
, d_running(false)
, d_wakeEvent(mtcore::Event::e_AUTO_RESET)
, d_logonEvent(mtcore::Event::e_MANUAL_RESET)
, d_managerRunnable(this, &Session::managerLoop)
, d_manager("session")
, d_nextCorrelationId(0)
, d_reconnects(0)
, d_connects(0)
{
}

Session::~Session()
{
    stop();
}

int Session::start()
{
    if (d_running) {
        return 0;
    }
    d_running = true;
    d_state   = SessionState::e_CONNECTING;
    return d_manager.start(&d_managerRunnable);
}

void Session::stop()
{
    if (!d_running) {
        return;
    }
    d_running = false;
    d_wakeEvent.set();
    d_logonEvent.set();
    d_manager.join();
    ConnectionHandle conn;
    {
        LockGuard guard(&d_mutex);
        conn = d_connection;
        d_connection.reset();
        d_state = SessionState::e_STOPPED;
    }
    if (!conn.isNull()) {
        conn->close();
        conn->join();
    }
}

Uint64 Session::nextCorrelationId()
{
    return (Uint64)d_nextCorrelationId.increment();
}

ConnectionHandle Session::connection() const
{
    LockGuard guard(&d_mutex);
    return d_connection;
}

void Session::managerLoop()
{
    int backoff = d_options.reconnectMinMs;
    while (d_running) {
        if (!connectOnce()) {
            if (!d_running) {
                break;
            }
            {
                LockGuard guard(&d_mutex);
                d_state = SessionState::e_DOWN;
            }
            expireRequests(TimeUtil::nowMillis());
            if (!d_options.autoReconnect) {
                d_handler_p->processSessionState(this,
                                                 SessionState::e_DOWN,
                                                 NULL);
                break;
            }
            d_wakeEvent.wait(backoff);
            backoff *= 2;
            if (backoff > d_options.reconnectMaxMs) {
                backoff = d_options.reconnectMaxMs;
            }
            continue;
        }
        backoff = d_options.reconnectMinMs;

        ConnectionHandle conn = connection();
        Int64 lastHeartbeat = TimeUtil::nowMillis();
        int hb = d_options.heartbeatIntervalMs;
        while (d_running && !conn.isNull() && conn->isOpen()) {
            d_wakeEvent.wait(hb > 0 && hb < 250 ? hb : 250);
            Int64 now = TimeUtil::nowMillis();
            if (hb > 0) {
                if (now - lastHeartbeat >= hb) {
                    conn->send(Message(MessageType::e_HEARTBEAT, 0));
                    lastHeartbeat = now;
                }
                if (now - conn->lastReceiveMillis() > 3 * (Int64)hb) {
                    MT_LOG_WARN("session: no traffic from %s:%d for %I64d ms",
                                d_options.host.c_str(),
                                d_options.port,
                                now - conn->lastReceiveMillis());
                    conn->close();
                }
            }
            flushQueuedRequests();
            expireRequests(now);
        }
        connectionLost();
        if (d_running && !d_options.autoReconnect) {
            break;
        }
    }
}

bool Session::connectOnce()
{
    mtcore::TcpSocket *socket = new mtcore::TcpSocket();
    if (socket->connect(d_options.host.c_str(), d_options.port) != 0) {
        MT_LOG_DEBUG("session: connect %s:%d failed",
                     d_options.host.c_str(),
                     d_options.port);
        delete socket;
        return false;
    }
    socket->setNoDelay(true);
    d_logonEvent.reset();
    Connection *raw = new Connection(socket, this, "session");
    ConnectionHandle conn(raw);
    {
        LockGuard guard(&d_mutex);
        d_connection = conn;
    }
    if (conn->start() != 0) {
        LockGuard guard(&d_mutex);
        d_connection.reset();
        return false;
    }

    Message logon(MessageType::e_LOGON, 0);
    logon.body().setChildString("user", d_options.user);
    logon.body().setChildString("application", d_options.application);
    logon.body().setChildInt64("conflateMs", d_options.conflateMs);
    conn->send(logon);

    bool signalled = d_logonEvent.wait(d_options.logonTimeoutMs);
    if (!signalled || !d_running || !conn->isOpen()) {
        MT_LOG_WARN("session: logon to %s:%d failed",
                    d_options.host.c_str(),
                    d_options.port);
        conn->close();
        conn->join();
        LockGuard guard(&d_mutex);
        d_connection.reset();
        return false;
    }

    Message ack;
    int restored = 0;
    {
        LockGuard guard(&d_mutex);
        ack     = d_logonAck;
        d_state = SessionState::e_UP;
        for (SubscriptionMap::iterator it = d_subscriptions.begin();
             it != d_subscriptions.end();
             ++it) {
            conn->send(it->second);
            ++restored;
        }
    }
    if (d_connects.increment() > 1) {
        d_reconnects.increment();
    }
    flushQueuedRequests();
    MT_LOG_INFO("session: logged on to %s:%d (%d subscriptions restored)",
                d_options.host.c_str(),
                d_options.port,
                restored);
    d_handler_p->processSessionState(this, SessionState::e_UP, &ack);
    return true;
}

void Session::connectionLost()
{
    ConnectionHandle conn;
    bool wasUp = false;
    std::vector<Message> failed;
    {
        LockGuard guard(&d_mutex);
        conn = d_connection;
        d_connection.reset();
        wasUp = d_state == SessionState::e_UP;
        if (d_running) {
            d_state = SessionState::e_DOWN;
        }
        RequestMap::iterator it = d_requests.begin();
        while (it != d_requests.end()) {
            if (it->second.d_sent) {
                failed.push_back(it->second.d_request);
                d_requests.erase(it++);
            }
            else {
                ++it;
            }
        }
    }
    if (!conn.isNull()) {
        conn->close();
        conn->join();
    }
    for (size_t i = 0; i < failed.size(); ++i) {
        failRequest(failed[i], "CONNECTION_LOST", "connection lost");
    }
    if (wasUp && d_running) {
        MT_LOG_WARN("session: connection to %s:%d lost",
                    d_options.host.c_str(),
                    d_options.port);
        d_handler_p->processSessionState(this, SessionState::e_DOWN, NULL);
    }
}

void Session::failRequest(const Message&     request,
                          const char        *category,
                          const std::string& text)
{
    Message failure(MessageType::e_REQUEST_FAILURE, request.correlationId());
    failure.setService(request.service());
    failure.setOperation(request.operation());
    mtcore::Element *reason = failure.body().addChild("reason");
    reason->setChildInt64("code", 200);
    reason->setChildString("category", category);
    reason->setChildString("message", text);
    d_handler_p->processMessage(this, failure);
}

void Session::expireRequests(Int64 nowMs)
{
    if (d_options.requestTimeoutMs <= 0) {
        return;
    }
    std::vector<Message> expired;
    {
        LockGuard guard(&d_mutex);
        RequestMap::iterator it = d_requests.begin();
        while (it != d_requests.end()) {
            if (nowMs - it->second.d_submittedMs
                                           > d_options.requestTimeoutMs) {
                expired.push_back(it->second.d_request);
                d_requests.erase(it++);
            }
            else {
                ++it;
            }
        }
    }
    for (size_t i = 0; i < expired.size(); ++i) {
        failRequest(expired[i], "TIMEOUT", "request timed out");
    }
}

void Session::flushQueuedRequests()
{
    LockGuard guard(&d_mutex);
    if (d_state != SessionState::e_UP || d_connection.isNull()) {
        return;
    }
    for (RequestMap::iterator it = d_requests.begin();
         it != d_requests.end();
         ++it) {
        if (!it->second.d_sent) {
            d_connection->send(it->second.d_request);
            it->second.d_sent = true;
        }
    }
}

int Session::subscribe(const std::string&              service,
                       const std::string&              topic,
                       const std::vector<std::string>& fields,
                       Uint64                          correlationId)
{
    Message msg(MessageType::e_SUBSCRIBE, correlationId);
    msg.setService(service);
    msg.setTopic(topic);
    if (!fields.empty()) {
        mtcore::Element *list = msg.body().addChild("fields");
        list->setArray(true);
        for (std::vector<std::string>::const_iterator it = fields.begin();
             it != fields.end();
             ++it) {
            list->appendItem()->setString(*it);
        }
    }
    return subscribe(msg);
}

int Session::subscribe(const Message& subscribe)
{
    LockGuard guard(&d_mutex);
    if (d_subscriptions.find(subscribe.correlationId())
                                                  != d_subscriptions.end()) {
        return -1;
    }
    d_subscriptions[subscribe.correlationId()] = subscribe;
    if (d_state == SessionState::e_UP && !d_connection.isNull()) {
        d_connection->send(subscribe);
    }
    return 0;
}

int Session::unsubscribe(const std::string& service, Uint64 correlationId)
{
    LockGuard guard(&d_mutex);
    SubscriptionMap::iterator it = d_subscriptions.find(correlationId);
    if (it == d_subscriptions.end()) {
        return -1;
    }
    d_subscriptions.erase(it);
    if (d_state == SessionState::e_UP && !d_connection.isNull()) {
        Message msg(MessageType::e_UNSUBSCRIBE, correlationId);
        msg.setService(service);
        d_connection->send(msg);
    }
    return 0;
}

int Session::sendRequest(const Message& request)
{
    LockGuard guard(&d_mutex);
    if (d_requests.find(request.correlationId()) != d_requests.end()) {
        return -1;
    }
    PendingRequest& pending = d_requests[request.correlationId()];
    pending.d_request     = request;
    pending.d_submittedMs = TimeUtil::nowMillis();
    pending.d_sent        = false;
    if (d_state == SessionState::e_UP && !d_connection.isNull()) {
        d_connection->send(request);
        pending.d_sent = true;
    }
    return 0;
}

void Session::onMessage(Connection *connection, const Message& message)
{
    switch (message.type()) {
      case MessageType::e_LOGON_ACK: {
        LockGuard guard(&d_mutex);
        d_logonAck = message;
        d_logonEvent.set();
      } return;
      case MessageType::e_HEARTBEAT:
        connection->send(Message(MessageType::e_HEARTBEAT_ACK,
                                 message.correlationId()));
        return;
      case MessageType::e_HEARTBEAT_ACK:
        return;
      case MessageType::e_RESPONSE:
      case MessageType::e_REQUEST_FAILURE: {
        LockGuard guard(&d_mutex);
        RequestMap::iterator it = d_requests.find(message.correlationId());
        if (it == d_requests.end()) {
            return;   // timed out or failed already
        }
        d_requests.erase(it);
      } break;
      case MessageType::e_PARTIAL_RESPONSE: {
        LockGuard guard(&d_mutex);
        if (d_requests.find(message.correlationId()) == d_requests.end()) {
            return;
        }
      } break;
      case MessageType::e_SUBSCRIPTION_FAILURE:
      case MessageType::e_SUBSCRIPTION_TERMINATED: {
        LockGuard guard(&d_mutex);
        if (d_subscriptions.erase(message.correlationId()) == 0) {
            return;
        }
      } break;
      case MessageType::e_SUBSCRIPTION_STARTED:
      case MessageType::e_SUBSCRIPTION_DATA: {
        LockGuard guard(&d_mutex);
        if (d_subscriptions.find(message.correlationId())
                                                  == d_subscriptions.end()) {
            return;   // raced with unsubscribe
        }
      } break;
      default:
        break;
    }
    d_handler_p->processMessage(this, message);
}

void Session::onDisconnect(Connection *)
{
    d_logonEvent.set();
    d_wakeEvent.set();
}

SessionState::Value Session::state() const
{
    return d_state;
}

bool Session::isUp() const
{
    return d_state == SessionState::e_UP;
}

const SessionOptions& Session::options() const
{
    return d_options;
}

int Session::numSubscriptions() const
{
    LockGuard guard(&d_mutex);
    return (int)d_subscriptions.size();
}

int Session::numPendingRequests() const
{
    LockGuard guard(&d_mutex);
    return (int)d_requests.size();
}

int Session::reconnectCount() const
{
    return d_reconnects.get();
}

int Session::connectCount() const
{
    return d_connects.get();
}

Int64 Session::messagesIn() const
{
    ConnectionHandle conn = connection();
    return conn.isNull() ? 0 : conn->messagesIn();
}

Int64 Session::messagesOut() const
{
    ConnectionHandle conn = connection();
    return conn.isNull() ? 0 : conn->messagesOut();
}

}  // close namespace mtclient
