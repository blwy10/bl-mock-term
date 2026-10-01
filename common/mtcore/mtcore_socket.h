// mtcore_socket.h                                                    -*-C++-*-
#ifndef INCLUDED_MTCORE_SOCKET
#define INCLUDED_MTCORE_SOCKET

//@PURPOSE: Provide blocking TCP stream and listener sockets over Winsock.
//
//@CLASSES:
//  mtcore::SocketUtil:   Winsock initialisation and error helpers
//  mtcore::TcpSocket:    connected stream socket
//  mtcore::TcpListener:  bound, listening socket

#include "mtcore_platform.h"

#include <string>

namespace mtcore {

struct SocketUtil {
    static int initialize();
        // Call 'WSAStartup' (reference counted).  Return 0 on success.
    static void cleanup();
    static int lastError();
    static std::string errorText(int error);
};

class TcpSocket {
    SOCKET      d_socket;
    std::string d_peer;

  private:
    // NOT IMPLEMENTED
    TcpSocket(const TcpSocket&);
    TcpSocket& operator=(const TcpSocket&);

  public:
    TcpSocket();
    explicit TcpSocket(SOCKET socket, const std::string& peer = "");
    ~TcpSocket();

    int connect(const char *host, int port);
        // Resolve 'host' and connect.  Return 0 on success.

    int sendAll(const char *data, int length);
        // Return 0 once every byte is written, non-zero on error.
    int receive(char *buffer, int capacity);
        // Return bytes read, 0 on orderly close, negative on error.
    int receiveAll(char *buffer, int length);
        // Return 0 once 'length' bytes are read, non-zero otherwise.

    void setNoDelay(bool enabled);
    void setReceiveTimeout(int milliseconds);
    void shutdown();
        // Shut down both directions; blocked reads return 0.
    void close();

    bool isValid() const;
    const std::string& peer() const;
};

class TcpListener {
    SOCKET d_socket;
    int    d_port;

  private:
    // NOT IMPLEMENTED
    TcpListener(const TcpListener&);
    TcpListener& operator=(const TcpListener&);

  public:
    TcpListener();
    ~TcpListener();

    int listen(const char *interfaceAddress, int port, int backlog = 16);
        // Bind and listen.  'port' 0 selects an ephemeral port; see 'port'.
        // Return 0 on success.

    TcpSocket *accept();
        // Block until a connection arrives and return a new socket (owned by
        // the caller), or NULL if the listener was closed.

    void close();
    int port() const;
    bool isValid() const;
};

}  // close namespace mtcore

#endif
