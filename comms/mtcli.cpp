// mtcli.cpp                                                          -*-C++-*-
//
// Command-line client for mtcomm (or mtserver directly).  Useful for demos,
// smoke tests and debugging the wire protocol:
//..
//  mtcli status
//  mtcli snapshot "AAPL US Equity" "MSFT US Equity"
//  mtcli refdata "MSFT US Equity" NAME SECTOR PE_RATIO
//  mtcli history "MSFT US Equity" 1M
//  mtcli news fed
//  mtcli search apple
//  mtcli send ALICE "hello"
//  mtcli subscribe "EURUSD Curncy" 5
//..

#include <mtclient_session.h>
#include <mtcore_log.h>
#include <mtcore_mutex.h>
#include <mtcore_socket.h>
#include <mtcore_stringutil.h>
#include <mtcore_time.h>

#include <deque>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace {

using mtcore::Element;
using mtcore::Int64;
using mtcore::LockGuard;
using mtcore::Message;
using mtcore::MessageFlags;
using mtcore::MessageType;
using mtcore::Uint64;

class QueueHandler : public mtclient::EventHandler {
    mtcore::Mutex       d_mutex;
    mtcore::Event       d_event;
    mtcore::Event       d_upEvent;
    std::deque<Message> d_queue;
    Message             d_ack;

  public:
    QueueHandler()
    : d_event(mtcore::Event::e_AUTO_RESET)
    , d_upEvent(mtcore::Event::e_MANUAL_RESET)
    {
    }

    virtual void processMessage(mtclient::Session *, const Message& message)
    {
        LockGuard guard(&d_mutex);
        d_queue.push_back(message);
        d_event.set();
    }

    virtual void processSessionState(mtclient::Session            *,
                                     mtclient::SessionState::Value state,
                                     const Message                *ack)
    {
        if (state == mtclient::SessionState::e_UP) {
            {
                LockGuard guard(&d_mutex);
                d_ack = *ack;
            }
            d_upEvent.set();
        }
        else {
            fprintf(stderr, "mtcli: session %s\n",
                    mtclient::SessionState::toString(state));
        }
    }

    bool waitUp(int timeoutMs)
    {
        return d_upEvent.wait(timeoutMs);
    }

    Message ack()
    {
        LockGuard guard(&d_mutex);
        return d_ack;
    }

    bool next(Message *result, int timeoutMs)
    {
        Int64 deadline = mtcore::TimeUtil::nowMillis() + timeoutMs;
        for (;;) {
            {
                LockGuard guard(&d_mutex);
                if (!d_queue.empty()) {
                    *result = d_queue.front();
                    d_queue.pop_front();
                    return true;
                }
            }
            Int64 left = deadline - mtcore::TimeUtil::nowMillis();
            if (left <= 0) {
                return false;
            }
            d_event.wait((int)left);
        }
    }
};

void printMessage(const Message& message)
{
    std::cout << message.describe() << "\n";
    message.body().print(std::cout, 1);
    std::cout.flush();
}

void usage()
{
    fprintf(stderr,
            "usage: mtcli [--host H] [--port N] [--conflate-ms N] COMMAND\n"
            "commands:\n"
            "  status                         mtcomm admin status\n"
            "  services                       services from the logon ack\n"
            "  snapshot TICKER...             //mt/mktdata Snapshot\n"
            "  refdata TICKER FIELD...        //mt/refdata ReferenceData\n"
            "  history TICKER [1D|1M|1Y]      //mt/refdata HistoricalData\n"
            "  securities                     //mt/refdata SecurityList\n"
            "  search TEXT                    //mt/refdata SecuritySearch\n"
            "  fields                         //mt/refdata FieldList\n"
            "  news [QUERY]                   //mt/news Search\n"
            "  send TO TEXT                   //mt/msg Send\n"
            "  subscribe TICKER [SECONDS]     stream //mt/mktdata\n");
}

int runRequest(mtclient::Session *session,
               QueueHandler      *handler,
               Message           *request)
{
    request->setCorrelationId(session->nextCorrelationId());
    session->sendRequest(*request);
    for (;;) {
        Message msg;
        if (!handler->next(&msg, 20000)) {
            fprintf(stderr, "mtcli: timed out waiting for response\n");
            return 1;
        }
        if (msg.correlationId() != request->correlationId()) {
            continue;
        }
        printMessage(msg);
        if (msg.type() == MessageType::e_PARTIAL_RESPONSE) {
            continue;
        }
        return msg.type() == MessageType::e_RESPONSE ? 0 : 1;
    }
}

int runSubscribe(mtclient::Session  *session,
                 QueueHandler       *handler,
                 const std::string&  ticker,
                 int                 seconds)
{
    std::vector<std::string> fields;
    fields.push_back("LAST_PRICE");
    fields.push_back("BID");
    fields.push_back("ASK");
    fields.push_back("VOLUME");
    Uint64 cid = session->nextCorrelationId();
    session->subscribe("//mt/mktdata", ticker, fields, cid);
    Int64 end = mtcore::TimeUtil::nowMillis() + seconds * 1000;
    int updates = 0;
    for (;;) {
        Int64 left = end - mtcore::TimeUtil::nowMillis();
        if (left <= 0) {
            break;
        }
        Message msg;
        if (!handler->next(&msg, (int)left)) {
            break;
        }
        if (msg.type() == MessageType::e_SUBSCRIPTION_DATA) {
            ++updates;
            printf("%s %-20s last=%-10.4f bid=%-10.4f ask=%-10.4f%s%s\n",
                   mtcore::TimeUtil::formatIso(
                                  mtcore::TimeUtil::nowMillis()).c_str(),
                   msg.topic().c_str(),
                   msg.body().getChildFloat64("LAST_PRICE"),
                   msg.body().getChildFloat64("BID"),
                   msg.body().getChildFloat64("ASK"),
                   msg.hasFlag(MessageFlags::e_INITIAL_PAINT) ? " [paint]" : "",
                   msg.hasFlag(MessageFlags::e_CONFLATED) ? " [conflated]"
                                                          : "");
            fflush(stdout);
        }
        else {
            printMessage(msg);
            if (msg.type() == MessageType::e_SUBSCRIPTION_FAILURE) {
                return 1;
            }
        }
    }
    session->unsubscribe("//mt/mktdata", cid);
    printf("%d updates\n", updates);
    return updates > 0 ? 0 : 1;
}

void addList(Element *parent, const char *name, int argc, char *argv[])
{
    Element *list = parent->addChild(name);
    list->setArray(true);
    for (int i = 0; i < argc; ++i) {
        list->appendItem()->setString(argv[i]);
    }
}

}  // close unnamed namespace

int main(int argc, char *argv[])
{
    mtclient::SessionOptions options;
    options.application         = "mtcli";
    options.autoReconnect       = false;
    options.heartbeatIntervalMs = 0;

    int i = 1;
    for (; i + 1 < argc && !strncmp(argv[i], "--", 2); i += 2) {
        if (!strcmp(argv[i], "--host")) {
            options.host = argv[i + 1];
        }
        else if (!strcmp(argv[i], "--port")) {
            options.port = atoi(argv[i + 1]);
        }
        else if (!strcmp(argv[i], "--conflate-ms")) {
            options.conflateMs = atoi(argv[i + 1]);
        }
        else if (!strcmp(argv[i], "--user")) {
            options.user = argv[i + 1];
        }
        else {
            usage();
            return 2;
        }
    }
    if (i >= argc) {
        usage();
        return 2;
    }
    std::string command = argv[i++];
    int nargs = argc - i;
    char **args = argv + i;

    mtcore::Logger::setProcessTag("mtcli");
    mtcore::Logger::setLevel(mtcore::LogLevel::e_WARN);
    mtcore::SocketUtil::initialize();

    QueueHandler handler;
    mtclient::Session session(options, &handler);
    session.start();
    if (!handler.waitUp(options.logonTimeoutMs + 1000)) {
        fprintf(stderr, "mtcli: cannot connect to %s:%d\n",
                options.host.c_str(), options.port);
        session.stop();
        mtcore::SocketUtil::cleanup();
        return 1;
    }

    int rc = 0;
    Message req(MessageType::e_REQUEST);
    if (command == "status") {
        req.setService("//mtcomm/admin");
        req.setOperation("Status");
        rc = runRequest(&session, &handler, &req);
    }
    else if (command == "services") {
        printMessage(handler.ack());
    }
    else if (command == "snapshot" && nargs >= 1) {
        req.setService("//mt/mktdata");
        req.setOperation("Snapshot");
        addList(&req.body(), "securities", nargs, args);
        rc = runRequest(&session, &handler, &req);
    }
    else if (command == "refdata" && nargs >= 2) {
        req.setService("//mt/refdata");
        req.setOperation("ReferenceData");
        addList(&req.body(), "securities", 1, args);
        addList(&req.body(), "fields", nargs - 1, args + 1);
        rc = runRequest(&session, &handler, &req);
    }
    else if (command == "history" && nargs >= 1) {
        req.setService("//mt/refdata");
        req.setOperation("HistoricalData");
        req.body().setChildString("security", args[0]);
        req.body().setChildString("range", nargs > 1 ? args[1] : "1D");
        rc = runRequest(&session, &handler, &req);
    }
    else if (command == "securities") {
        req.setService("//mt/refdata");
        req.setOperation("SecurityList");
        rc = runRequest(&session, &handler, &req);
    }
    else if (command == "search" && nargs >= 1) {
        req.setService("//mt/refdata");
        req.setOperation("SecuritySearch");
        req.body().setChildString("query", args[0]);
        rc = runRequest(&session, &handler, &req);
    }
    else if (command == "fields") {
        req.setService("//mt/refdata");
        req.setOperation("FieldList");
        rc = runRequest(&session, &handler, &req);
    }
    else if (command == "news") {
        req.setService("//mt/news");
        req.setOperation("Search");
        req.body().setChildString("query", nargs > 0 ? args[0] : "");
        req.body().setChildInt64("maxResults", 10);
        rc = runRequest(&session, &handler, &req);
    }
    else if (command == "send" && nargs >= 2) {
        req.setService("//mt/msg");
        req.setOperation("Send");
        req.body().setChildString("from", options.user);
        req.body().setChildString("to", args[0]);
        req.body().setChildString("text", args[1]);
        rc = runRequest(&session, &handler, &req);
    }
    else if (command == "subscribe" && nargs >= 1) {
        rc = runSubscribe(&session, &handler, args[0],
                          nargs > 1 ? atoi(args[1]) : 5);
    }
    else {
        usage();
        rc = 2;
    }

    session.stop();
    mtcore::SocketUtil::cleanup();
    mtcore::Logger::shutdown();
    return rc;
}
