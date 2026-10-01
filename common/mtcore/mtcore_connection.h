// mtcore_connection.h                                                -*-C++-*-
#ifndef INCLUDED_MTCORE_CONNECTION
#define INCLUDED_MTCORE_CONNECTION

//@PURPOSE: Provide a framed, threaded, message-oriented TCP connection.
//
//@CLASSES:
//  mtcore::ConnectionHandler:  callbacks for inbound messages and disconnect
//  mtcore::Connection:         reader thread + writer thread + send queue
//
//@DESCRIPTION: A 'Connection' owns a connected 'TcpSocket'.  'start' launches
// a reader thread that decodes frames and calls 'onMessage', and a writer
// thread that drains a queue of encoded frames.  'send' is thread-safe and
// never blocks on the network.  When the peer disconnects, a frame is
// malformed, or the send queue exceeds its limit (a slow consumer),
// 'onDisconnect' is called exactly once from the reader thread.
//
// Each running thread holds a reference, so a connection stays alive until
// both threads exit even if every 'Handle' is dropped.  Callbacks run on the
// reader thread; the handler must outlive the threads (call 'close' then
// 'join' before destroying the handler).

#include "mtcore_message.h"
#include "mtcore_mutex.h"
#include "mtcore_refcount.h"
#include "mtcore_socket.h"
#include "mtcore_thread.h"

#include <deque>
#include <string>

namespace mtcore {

class Connection;

class ConnectionHandler {
  public:
    virtual ~ConnectionHandler();
    virtual void onMessage(Connection *connection, const Message& message) = 0;
    virtual void onDisconnect(Connection *connection) = 0;
};

class Connection : public RefCounted {
    typedef std::deque<std::string> FrameQueue;

    TcpSocket                  *d_socket_p;        // owned
    ConnectionHandler          *d_handler_p;       // held, not owned
    std::string                 d_name;
    Uint64                      d_id;
    MemberRunnable<Connection>  d_readerRunnable;
    MemberRunnable<Connection>  d_writerRunnable;
    Thread                      d_reader;
    Thread                      d_writer;
    Mutex                       d_queueMutex;
    Event                       d_queueEvent;
    FrameQueue                  d_queue;
    size_t                      d_maxQueue;
    volatile bool               d_open;
    AtomicInt                   d_disconnectNotified;
    volatile LONGLONG           d_messagesIn;
    volatile LONGLONG           d_messagesOut;
    volatile LONGLONG           d_bytesIn;
    volatile LONGLONG           d_bytesOut;
    volatile LONGLONG           d_lastReceiveMs;
    void                       *d_userData_p;

  private:
    void readerLoop();
    void writerLoop();
    void notifyDisconnect();

    // NOT IMPLEMENTED
    Connection(const Connection&);
    Connection& operator=(const Connection&);

  protected:
    virtual ~Connection();

  public:
    Connection(TcpSocket         *socket,
               ConnectionHandler *handler,
               const std::string& name,
               size_t             maxQueuedFrames = 200000);
        // Take ownership of the connected 'socket'.

    int start();
    bool send(const Message& message);
        // Encode and enqueue.  Return 'false' if the connection is closed.
    bool sendFrame(const std::string& frame);
    void close();
        // Idempotent.  Shuts the socket down and wakes both threads.
    void join();
        // Wait for both threads.  Must not be called from a callback.

    void setUserData(void *data);
    void *userData() const;

    bool isOpen() const;
    Uint64 id() const;
    const std::string& name() const;
    const std::string& peer() const;
    Int64 messagesIn() const;
    Int64 messagesOut() const;
    Int64 bytesIn() const;
    Int64 bytesOut() const;
    Int64 lastReceiveMillis() const;
    size_t queuedFrames();
};

typedef Handle<Connection> ConnectionHandle;

}  // close namespace mtcore

#endif
