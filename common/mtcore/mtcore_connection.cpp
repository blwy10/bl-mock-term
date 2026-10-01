// mtcore_connection.cpp                                              -*-C++-*-
#include "mtcore_connection.h"

#include "mtcore_codec.h"
#include "mtcore_log.h"
#include "mtcore_time.h"

#include <vector>

namespace mtcore {

namespace {

AtomicInt s_nextConnectionId;

}  // close unnamed namespace

ConnectionHandler::~ConnectionHandler()
{
}

Connection::Connection(TcpSocket         *socket,
                       ConnectionHandler *handler,
                       const std::string& name,
                       size_t             maxQueuedFrames)
: d_socket_p(socket)
, d_handler_p(handler)
, d_name(name)
, d_id((Uint64)s_nextConnectionId.increment())
, d_readerRunnable(this, &Connection::readerLoop)
, d_writerRunnable(this, &Connection::writerLoop)
, d_reader("reader")
, d_writer("writer")
, d_queueEvent(Event::e_AUTO_RESET)
, d_maxQueue(maxQueuedFrames)
, d_open(false)
, d_messagesIn(0)
, d_messagesOut(0)
, d_bytesIn(0)
, d_bytesOut(0)
, d_lastReceiveMs(0)
, d_userData_p(NULL)
{
}

Connection::~Connection()
{
    delete d_socket_p;
}

int Connection::start()
{
    d_open          = true;
    d_lastReceiveMs = TimeUtil::nowMillis();
    acquire();  // released by readerLoop
    acquire();  // released by writerLoop
    if (d_reader.start(&d_readerRunnable) != 0) {
        release();
        release();
        d_open = false;
        return -1;
    }
    if (d_writer.start(&d_writerRunnable) != 0) {
        release();
        close();
        return -2;
    }
    return 0;
}

bool Connection::send(const Message& message)
{
    std::string frame;
    MessageCodec::encode(&frame, message);
    return sendFrame(frame);
}

bool Connection::sendFrame(const std::string& frame)
{
    if (!d_open) {
        return false;
    }
    bool overflow = false;
    {
        LockGuard guard(&d_queueMutex);
        if (d_queue.size() >= d_maxQueue) {
            overflow = true;
        }
        else {
            d_queue.push_back(frame);
        }
    }
    if (overflow) {
        MT_LOG_WARN("connection %I64u (%s): send queue full (%u frames), "
                    "disconnecting slow consumer",
                    d_id,
                    d_name.c_str(),
                    (unsigned)d_maxQueue);
        close();
        return false;
    }
    d_queueEvent.set();
    return true;
}

void Connection::close()
{
    d_open = false;
    if (d_socket_p) {
        d_socket_p->shutdown();
    }
    d_queueEvent.set();
}

void Connection::join()
{
    d_reader.join();
    d_writer.join();
}

void Connection::notifyDisconnect()
{
    if (d_disconnectNotified.testAndSwap(0, 1) == 0 && d_handler_p) {
        d_handler_p->onDisconnect(this);
    }
}

void Connection::readerLoop()
{
    std::vector<char> payload;
    char header[MessageCodec::k_HEADER_SIZE];
    Message message;
    while (d_open) {
        if (d_socket_p->receiveAll(header, sizeof(header)) != 0) {
            break;
        }
        Uint32 length = 0;
        if (MessageCodec::frameLength(&length, header) != 0) {
            MT_LOG_WARN("connection %I64u (%s): bad frame length",
                        d_id,
                        d_name.c_str());
            break;
        }
        payload.resize(length);
        if (d_socket_p->receiveAll(&payload[0], (int)length) != 0) {
            break;
        }
        int rc = MessageCodec::decode(&message, &payload[0], length);
        if (rc != 0) {
            MT_LOG_WARN("connection %I64u (%s): malformed frame (rc=%d)",
                        d_id,
                        d_name.c_str(),
                        rc);
            break;
        }
        ::InterlockedIncrement64(&d_messagesIn);
        ::InterlockedExchangeAdd64(&d_bytesIn, length + sizeof(header));
        ::InterlockedExchange64(&d_lastReceiveMs, TimeUtil::nowMillis());
        if (d_handler_p) {
            d_handler_p->onMessage(this, message);
        }
    }
    close();
    notifyDisconnect();
    release();
}

void Connection::writerLoop()
{
    FrameQueue batch;
    for (;;) {
        d_queueEvent.wait(1000);
        {
            LockGuard guard(&d_queueMutex);
            batch.swap(d_queue);
        }
        if (!d_open) {
            break;
        }
        for (FrameQueue::iterator it = batch.begin(); it != batch.end(); ++it) {
            if (d_socket_p->sendAll(it->data(), (int)it->size()) != 0) {
                close();
                break;
            }
            ::InterlockedIncrement64(&d_messagesOut);
            ::InterlockedExchangeAdd64(&d_bytesOut, (LONGLONG)it->size());
        }
        batch.clear();
        if (!d_open) {
            break;
        }
    }
    release();
}

void Connection::setUserData(void *data)
{
    d_userData_p = data;
}

void *Connection::userData() const
{
    return d_userData_p;
}

bool Connection::isOpen() const
{
    return d_open;
}

Uint64 Connection::id() const
{
    return d_id;
}

const std::string& Connection::name() const
{
    return d_name;
}

const std::string& Connection::peer() const
{
    return d_socket_p->peer();
}

Int64 Connection::messagesIn() const
{
    return d_messagesIn;
}

Int64 Connection::messagesOut() const
{
    return d_messagesOut;
}

Int64 Connection::bytesIn() const
{
    return d_bytesIn;
}

Int64 Connection::bytesOut() const
{
    return d_bytesOut;
}

Int64 Connection::lastReceiveMillis() const
{
    return d_lastReceiveMs;
}

size_t Connection::queuedFrames()
{
    LockGuard guard(&d_queueMutex);
    return d_queue.size();
}

}  // close namespace mtcore
