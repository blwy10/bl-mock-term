// mtcore_socket.cpp                                                  -*-C++-*-
#include "mtcore_socket.h"

#include "mtcore_mutex.h"
#include "mtcore_stringutil.h"

#include <string.h>

namespace mtcore {

namespace {

AtomicInt s_initCount;

}  // close unnamed namespace

                                // ----------------
                                // class SocketUtil
                                // ----------------

int SocketUtil::initialize()
{
    if (s_initCount.increment() > 1) {
        return 0;
    }
    WSADATA data;
    return ::WSAStartup(MAKEWORD(2, 2), &data);
}

void SocketUtil::cleanup()
{
    if (s_initCount.decrement() == 0) {
        ::WSACleanup();
    }
}

int SocketUtil::lastError()
{
    return ::WSAGetLastError();
}

std::string SocketUtil::errorText(int error)
{
    char buffer[256];
    DWORD n = ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM
                                   | FORMAT_MESSAGE_IGNORE_INSERTS,
                               NULL,
                               (DWORD)error,
                               0,
                               buffer,
                               sizeof(buffer),
                               NULL);
    while (n > 0 && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r')) {
        --n;
    }
    return StringUtil::format("%d (%.*s)", error, (int)n, buffer);
}

                                // ---------------
                                // class TcpSocket
                                // ---------------

TcpSocket::TcpSocket()
: d_socket(INVALID_SOCKET)
{
}

TcpSocket::TcpSocket(SOCKET socket, const std::string& peer)
: d_socket(socket)
, d_peer(peer)
{
}

TcpSocket::~TcpSocket()
{
    close();
}

int TcpSocket::connect(const char *host, int port)
{
    close();
    struct hostent *he = ::gethostbyname(host);
    if (he == NULL || he->h_addrtype != AF_INET) {
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u_short)port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(addr.sin_addr));

    d_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (d_socket == INVALID_SOCKET) {
        return -2;
    }
    if (::connect(d_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close();
        return -3;
    }
    d_peer = StringUtil::format("%s:%d", host, port);
    setNoDelay(true);
    return 0;
}

int TcpSocket::sendAll(const char *data, int length)
{
    int sent = 0;
    while (sent < length) {
        int n = ::send(d_socket, data + sent, length - sent, 0);
        if (n == SOCKET_ERROR || n == 0) {
            return -1;
        }
        sent += n;
    }
    return 0;
}

int TcpSocket::receive(char *buffer, int capacity)
{
    int n = ::recv(d_socket, buffer, capacity, 0);
    if (n == SOCKET_ERROR) {
        return -1;
    }
    return n;
}

int TcpSocket::receiveAll(char *buffer, int length)
{
    int got = 0;
    while (got < length) {
        int n = receive(buffer + got, length - got);
        if (n <= 0) {
            return -1;
        }
        got += n;
    }
    return 0;
}

void TcpSocket::setNoDelay(bool enabled)
{
    BOOL flag = enabled ? TRUE : FALSE;
    ::setsockopt(d_socket,
                 IPPROTO_TCP,
                 TCP_NODELAY,
                 (const char *)&flag,
                 sizeof(flag));
}

void TcpSocket::setReceiveTimeout(int milliseconds)
{
    DWORD timeout = (DWORD)milliseconds;
    ::setsockopt(d_socket,
                 SOL_SOCKET,
                 SO_RCVTIMEO,
                 (const char *)&timeout,
                 sizeof(timeout));
}

void TcpSocket::shutdown()
{
    if (d_socket != INVALID_SOCKET) {
        ::shutdown(d_socket, SD_BOTH);
    }
}

void TcpSocket::close()
{
    if (d_socket != INVALID_SOCKET) {
        ::closesocket(d_socket);
        d_socket = INVALID_SOCKET;
    }
}

bool TcpSocket::isValid() const
{
    return d_socket != INVALID_SOCKET;
}

const std::string& TcpSocket::peer() const
{
    return d_peer;
}

                                // -----------------
                                // class TcpListener
                                // -----------------

TcpListener::TcpListener()
: d_socket(INVALID_SOCKET)
, d_port(0)
{
}

TcpListener::~TcpListener()
{
    close();
}

int TcpListener::listen(const char *interfaceAddress, int port, int backlog)
{
    close();
    d_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (d_socket == INVALID_SOCKET) {
        return -1;
    }
    BOOL exclusive = TRUE;
    ::setsockopt(d_socket,
                 SOL_SOCKET,
                 SO_EXCLUSIVEADDRUSE,
                 (const char *)&exclusive,
                 sizeof(exclusive));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((u_short)port);
    addr.sin_addr.s_addr = ::inet_addr(interfaceAddress);
    if (::bind(d_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close();
        return -2;
    }
    if (::listen(d_socket, backlog) != 0) {
        close();
        return -3;
    }
    int len = sizeof(addr);
    ::getsockname(d_socket, (struct sockaddr *)&addr, &len);
    d_port = ntohs(addr.sin_port);
    return 0;
}

TcpSocket *TcpListener::accept()
{
    struct sockaddr_in addr;
    int len = sizeof(addr);
    SOCKET s = ::accept(d_socket, (struct sockaddr *)&addr, &len);
    if (s == INVALID_SOCKET) {
        return NULL;
    }
    std::string peer = StringUtil::format("%s:%d",
                                          ::inet_ntoa(addr.sin_addr),
                                          (int)ntohs(addr.sin_port));
    TcpSocket *socket = new TcpSocket(s, peer);
    socket->setNoDelay(true);
    return socket;
}

void TcpListener::close()
{
    if (d_socket != INVALID_SOCKET) {
        ::closesocket(d_socket);
        d_socket = INVALID_SOCKET;
    }
}

int TcpListener::port() const
{
    return d_port;
}

bool TcpListener::isValid() const
{
    return d_socket != INVALID_SOCKET;
}

}  // close namespace mtcore
