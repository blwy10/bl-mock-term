// mtcomm_engine.h                                                    -*-C++-*-
#ifndef INCLUDED_MTCOMM_ENGINE
#define INCLUDED_MTCOMM_ENGINE

//@PURPOSE: Provide the local communications engine (the bbcomm analogue).
//
//@CLASSES:
//  mtcomm::CommsEngine:  local listener, upstream session and routing
//
//@DESCRIPTION: 'CommsEngine' accepts local client connections, holds a
// single reconnecting upstream 'mtclient::Session' to mtserver, and routes
// between them:
//
//: o requests are forwarded with a fresh upstream correlation id and the
//:   response is mapped back to the originating client;
//: o identical subscriptions from any number of clients share one upstream
//:   subscription ('TopicMux'), with a local last-value cache providing an
//:   immediate initial paint for late joiners;
//: o clients that request conflation receive merged updates at most once
//:   per interval ('Conflator');
//: o upstream outages are broadcast as 'e_SESSION_STATUS', the cache is
//:   marked stale, and on reconnect subscriptions are restored and the fresh
//:   image is delivered flagged 'e_RECAP'.
//
// The service "//mtcomm/admin" (operation "Status") is answered locally.
// With '--spawn-server' the engine also supervises a child mtserver and
// restarts it if it exits.

#include "mtcomm_config.h"
#include "mtcomm_conflator.h"
#include "mtcomm_localclient.h"
#include "mtcomm_lvc.h"
#include "mtcomm_topicmux.h"

#include <mtclient_session.h>
#include <mtcore_connection.h>
#include <mtcore_mutex.h>
#include <mtcore_process.h>
#include <mtcore_socket.h>
#include <mtcore_thread.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mtcomm {

class CommsEngine : public mtcore::ConnectionHandler,
                    public mtclient::EventHandler {
    typedef std::map<Uint64, LocalClientHandle>       ClientMap;
    typedef std::map<Uint64, DownstreamKey>           RequestRouteMap;
    typedef std::vector<mtcore::ConnectionHandle>     ClosedList;

    CommsConfig                          d_config;
    std::auto_ptr<mtclient::Session>     d_upstream_p;
    std::auto_ptr<mtcore::ChildProcess>  d_server_p;
    mtcore::TcpListener                  d_listener;
    mtcore::MemberRunnable<CommsEngine>  d_acceptRunnable;
    mtcore::MemberRunnable<CommsEngine>  d_timerRunnable;
    mtcore::Thread                       d_acceptThread;
    mtcore::Thread                       d_timerThread;
    mtcore::Event                        d_stopEvent;
    volatile bool                        d_running;
    Int64                                d_startTimeMs;

    mutable mtcore::Mutex                d_clientMutex;
    ClientMap                            d_clientsByConnection;
    ClientMap                            d_clientsById;
    ClosedList                           d_closed;
    mtcore::AtomicInt                    d_nextClientId;

    mutable mtcore::Mutex                d_routeMutex;   // before client mutex
    TopicMux                             d_mux;
    RequestRouteMap                      d_requestRoutes;
    mtcore::Message                      d_upstreamAck;

    LastValueCache                       d_cache;
    Conflator                            d_conflator;

    volatile LONGLONG                    d_updatesIn;
    volatile LONGLONG                    d_updatesOut;
    volatile LONGLONG                    d_requestsRouted;
    volatile LONGLONG                    d_cacheHits;
    volatile LONGLONG                    d_recaps;
    volatile LONGLONG                    d_upstreamOutages;
    volatile LONGLONG                    d_totalClients;
    volatile LONGLONG                    d_serverRestarts;

  private:
    void acceptLoop();
    void timerLoop();
    void superviseServer();
    void flushConflated(Int64 nowMs);
    void checkClients(Int64 nowMs);
    void reapClosed();
    void logStats();
    int spawnServer();

    LocalClientHandle findClient(mtcore::Connection *connection) const;
    LocalClientHandle findClientById(Uint64 clientId) const;
    void broadcast(const mtcore::Message& message);

    void handleLogon(LocalClient *client, const mtcore::Message& logon);
    void handleRequest(LocalClient *client, const mtcore::Message& request);
    void handleSubscribe(LocalClient *client, const mtcore::Message& msg);
    void handleUnsubscribe(LocalClient *client, const mtcore::Message& msg);
    void handleAdmin(LocalClient *client, const mtcore::Message& request);
    void reject(LocalClient            *client,
                const mtcore::Message&  request,
                const char             *category,
                const std::string&      text);

    void routeResponse(const mtcore::Message& message);
    void routeSubscriptionStarted(const mtcore::Message& message);
    void routeSubscriptionData(const mtcore::Message& message);
    void routeSubscriptionEnd(const mtcore::Message& message);

    // NOT IMPLEMENTED
    CommsEngine(const CommsEngine&);
    CommsEngine& operator=(const CommsEngine&);

  public:
    static const char k_ADMIN_SERVICE[];

    explicit CommsEngine(const CommsConfig& config);
    virtual ~CommsEngine();

    int start();
    void stop();

    int port() const;
    int clientCount() const;
    int topicCount() const;
    bool isUpstreamUp() const;
    mtclient::Session *upstream() const;
    const LastValueCache& cache() const;
    const Conflator& conflator() const;
    void buildStatus(mtcore::Element *status) const;

    // mtcore::ConnectionHandler (local clients)
    virtual void onMessage(mtcore::Connection    *connection,
                           const mtcore::Message& message);
    virtual void onDisconnect(mtcore::Connection *connection);

    // mtclient::EventHandler (upstream)
    virtual void processMessage(mtclient::Session     *session,
                                const mtcore::Message& message);
    virtual void processSessionState(mtclient::Session          *session,
                                     mtclient::SessionState::Value state,
                                     const mtcore::Message       *logonAck);
};

}  // close namespace mtcomm

#endif
