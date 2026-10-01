// mtclient_session.h                                                 -*-C++-*-
#ifndef INCLUDED_MTCLIENT_SESSION
#define INCLUDED_MTCLIENT_SESSION

//@PURPOSE: Provide a reconnecting client session for the MockTerm protocol.
//
//@CLASSES:
//  mtclient::SessionState:    connection state enumeration
//  mtclient::SessionOptions:  endpoint, identity and timing parameters
//  mtclient::EventHandler:    application callback protocol
//  mtclient::Session:         logon, heartbeats, requests, subscriptions
//
//@DESCRIPTION: Modelled loosely on a BLPAPI-style session.  The session owns
// a manager thread that connects, logs on, heartbeats and (optionally)
// reconnects with exponential back-off.  Subscriptions are remembered and
// re-sent after every reconnect; requests submitted while disconnected are
// queued until logon or until 'requestTimeoutMs' expires, and requests that
// were in flight when the connection dropped are failed with category
// "CONNECTION_LOST".  'EventHandler' callbacks arrive on internal threads and
// must be thread-safe.
//
///Usage
///-----
//..
//  mtclient::SessionOptions options;
//  options.port = 8194;
//  mtclient::Session session(options, &handler);
//  session.start();
//  std::vector<std::string> fields;
//  fields.push_back("LAST_PRICE");
//  session.subscribe("//mt/mktdata", "MSFT US Equity", fields,
//                    session.nextCorrelationId());
//..

#include <mtcore_connection.h>
#include <mtcore_message.h>
#include <mtcore_mutex.h>
#include <mtcore_thread.h>

#include <map>
#include <string>
#include <vector>

namespace mtclient {

using mtcore::Int64;
using mtcore::Uint64;

struct SessionState {
    enum Value {
        e_STOPPED    = 0,
        e_CONNECTING = 1,
        e_UP         = 2,
        e_DOWN       = 3
    };
    static const char *toString(Value value);
};

struct SessionOptions {
    std::string host;
    int         port;
    std::string user;
    std::string application;
    int         conflateMs;           // requested of mtcomm; 0 = off
    bool        autoReconnect;
    int         logonTimeoutMs;
    int         reconnectMinMs;
    int         reconnectMaxMs;
    int         heartbeatIntervalMs;  // 0 disables heartbeats
    int         requestTimeoutMs;

    SessionOptions();
};

class Session;

class EventHandler {
  public:
    virtual ~EventHandler();
    virtual void processMessage(Session               *session,
                                const mtcore::Message& message) = 0;
    virtual void processSessionState(Session               *session,
                                     SessionState::Value    state,
                                     const mtcore::Message *logonAck) = 0;
        // 'logonAck' is non-null only for 'e_UP'.
};

class Session : public mtcore::ConnectionHandler {
    struct PendingRequest {
        mtcore::Message d_request;
        Int64           d_submittedMs;
        bool            d_sent;
    };
    typedef std::map<Uint64, mtcore::Message> SubscriptionMap;
    typedef std::map<Uint64, PendingRequest>  RequestMap;

    SessionOptions                  d_options;
    EventHandler                   *d_handler_p;   // held, not owned
    mutable mtcore::Mutex           d_mutex;
    mtcore::ConnectionHandle        d_connection;
    SubscriptionMap                 d_subscriptions;
    RequestMap                      d_requests;
    volatile SessionState::Value    d_state;
    volatile bool                   d_running;
    mtcore::Event                   d_wakeEvent;
    mtcore::Event                   d_logonEvent;
    mtcore::Message                 d_logonAck;
    mtcore::MemberRunnable<Session> d_managerRunnable;
    mtcore::Thread                  d_manager;
    mtcore::AtomicInt               d_nextCorrelationId;
    mtcore::AtomicInt               d_reconnects;
    mtcore::AtomicInt               d_connects;

  private:
    void managerLoop();
    bool connectOnce();
    void connectionLost();
    void expireRequests(Int64 nowMs);
    void flushQueuedRequests();
    void failRequest(const mtcore::Message& request,
                     const char            *category,
                     const std::string&     text);
    mtcore::ConnectionHandle connection() const;

    // NOT IMPLEMENTED
    Session(const Session&);
    Session& operator=(const Session&);

  public:
    Session(const SessionOptions& options, EventHandler *handler);
    virtual ~Session();

    int start();
        // Begin connecting in the background.  Return 0 on success.
    void stop();

    Uint64 nextCorrelationId();

    int subscribe(const std::string&              service,
                  const std::string&              topic,
                  const std::vector<std::string>& fields,
                  Uint64                          correlationId);
    int subscribe(const mtcore::Message& subscribe);
        // Remember and (when connected) send.  Return non-zero if the
        // correlation id is already in use.
    int unsubscribe(const std::string& service, Uint64 correlationId);
    int sendRequest(const mtcore::Message& request);
        // Return non-zero if the correlation id is already outstanding.

    SessionState::Value state() const;
    bool isUp() const;
    const SessionOptions& options() const;
    int numSubscriptions() const;
    int numPendingRequests() const;
    int reconnectCount() const;
    int connectCount() const;
    Int64 messagesIn() const;
    Int64 messagesOut() const;

    // mtcore::ConnectionHandler
    virtual void onMessage(mtcore::Connection    *connection,
                           const mtcore::Message& message);
    virtual void onDisconnect(mtcore::Connection *connection);
};

}  // close namespace mtclient

#endif
