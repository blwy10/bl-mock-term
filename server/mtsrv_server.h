// mtsrv_server.h                                                     -*-C++-*-
#ifndef INCLUDED_MTSRV_SERVER
#define INCLUDED_MTSRV_SERVER

//@PURPOSE: Assemble the simulated remote server: transport, sessions,
// services and background engines.
//
//@CLASSES:
//  mtsrv::Server:  owns every component and routes messages to services
//
//@DESCRIPTION:
//..
//   accept thread ----> Connection (reader/writer threads) per client
//                            |
//                            v  onMessage
//   LOGON/HEARTBEAT ---- handled inline
//   SUBSCRIBE/UNSUB ---- Service::handleSubscribe (reader thread)
//   REQUEST -----------> ThreadPool ---> Service::handleRequest
//
//   timer thread: news + chat polling, heartbeat timeouts, statistics,
//                 reaping closed connections
//..

#include "mtsrv_chatengine.h"
#include "mtsrv_clientsession.h"
#include "mtsrv_config.h"
#include "mtsrv_newsengine.h"
#include "mtsrv_security.h"
#include "mtsrv_service.h"
#include "mtsrv_tickerplant.h"

#include <mtcore_connection.h>
#include <mtcore_mutex.h>
#include <mtcore_socket.h>
#include <mtcore_thread.h>
#include <mtcore_threadpool.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mtsrv {

class MktdataService;
class MsgService;
class NewsService;
class RefdataService;

class Server : public mtcore::ConnectionHandler {
    typedef std::map<Uint64, ClientSessionHandle> SessionMap;
    typedef std::map<std::string, Service *>      ServiceMap;
    typedef std::vector<mtcore::ConnectionHandle> ConnectionList;

    ServerConfig                     d_config;
    SecurityMaster                   d_master;
    std::auto_ptr<TickerPlant>       d_plant_p;
    std::auto_ptr<NewsEngine>        d_news_p;
    std::auto_ptr<ChatEngine>        d_chat_p;
    ServiceMap                       d_services;     // owned
    MktdataService                  *d_mktdata_p;    // in 'd_services'
    NewsService                     *d_newsService_p;
    MsgService                      *d_msgService_p;
    mtcore::TcpListener              d_listener;
    mtcore::ThreadPool               d_pool;
    mtcore::MemberRunnable<Server>   d_acceptRunnable;
    mtcore::MemberRunnable<Server>   d_timerRunnable;
    mtcore::Thread                   d_acceptThread;
    mtcore::Thread                   d_timerThread;
    mtcore::Event                    d_stopEvent;
    mutable mtcore::Mutex            d_sessionMutex;
    SessionMap                       d_sessions;     // by connection id
    ConnectionList                   d_closed;       // awaiting join
    volatile bool                    d_running;
    mtcore::AtomicInt                d_nextSessionId;
    Int64                            d_startMs;
    volatile LONGLONG                d_totalSessions;
    volatile LONGLONG                d_requests;

  private:
    void acceptLoop();
    void timerLoop();
    void checkHeartbeats(Int64 nowMs);
    void reapClosed();
    void logStatistics();
    void handleLogon(ClientSession *session, const mtcore::Message& logon);
    Service *findService(const std::string& name) const;
    void addService(Service *service);

    // NOT IMPLEMENTED
    Server(const Server&);
    Server& operator=(const Server&);

  public:
    explicit Server(const ServerConfig& config);
    virtual ~Server();

    int start();
        // Start the engines, bind the listener and begin accepting.
        // Return 0 on success.
    void stop();

    int port() const;
    int sessionCount() const;
    void sessions(std::vector<ClientSessionHandle> *result) const;
    void buildStatus(mtcore::Element *status) const;
    void listServices(mtcore::Element *services) const;

    TickerPlant& plant();
    NewsEngine& news();
    ChatEngine& chat();
    const ServerConfig& config() const;

    // ConnectionHandler
    virtual void onMessage(mtcore::Connection    *connection,
                           const mtcore::Message& message);
    virtual void onDisconnect(mtcore::Connection *connection);
};

}  // close namespace mtsrv

#endif
