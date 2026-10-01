// mtcore_message.h                                                   -*-C++-*-
#ifndef INCLUDED_MTCORE_MESSAGE
#define INCLUDED_MTCORE_MESSAGE

//@PURPOSE: Provide the envelope exchanged between clients, comms and server.
//
//@CLASSES:
//  mtcore::MessageType:   enumeration of envelope kinds
//  mtcore::MessageFlags:  bit flags carried on subscription data
//  mtcore::Message:       envelope = type + correlation id + routing + body
//
//@DESCRIPTION: Every frame on the wire decodes to one 'Message'.  Requests
// and subscriptions name a 'service' (e.g. "//mt/mktdata"); requests also
// carry an 'operation' (e.g. "HistoricalData") and subscriptions a 'topic'
// (e.g. "AAPL US Equity").  Replies and updates echo the originator's
// correlation id.

#include "mtcore_element.h"
#include "mtcore_platform.h"

#include <string>

namespace mtcore {

struct MessageType {
    enum Value {
        e_UNKNOWN                  = 0,
        e_LOGON                    = 1,
        e_LOGON_ACK                = 2,
        e_LOGOFF                   = 3,
        e_HEARTBEAT                = 4,
        e_HEARTBEAT_ACK            = 5,
        e_REQUEST                  = 10,
        e_RESPONSE                 = 11,
        e_PARTIAL_RESPONSE         = 12,
        e_REQUEST_FAILURE          = 13,
        e_SUBSCRIBE                = 20,
        e_UNSUBSCRIBE              = 21,
        e_SUBSCRIPTION_STARTED     = 22,
        e_SUBSCRIPTION_FAILURE     = 23,
        e_SUBSCRIPTION_DATA        = 24,
        e_SUBSCRIPTION_TERMINATED  = 25,
        e_SESSION_STATUS           = 30,
        e_SERVICE_STATUS           = 31
    };

    static const char *toString(Value value);
    static bool isValid(int value);
};

struct MessageFlags {
    enum {
        e_NONE          = 0,
        e_INITIAL_PAINT = 1 << 0,   // first image of a subscription
        e_RECAP         = 1 << 1,   // re-sent image after recovery
        e_STALE         = 1 << 2,   // data may be out of date
        e_CONFLATED     = 1 << 3    // one or more updates were merged
    };
};

class Message {
    MessageType::Value d_type;
    Uint64             d_correlationId;
    Uint32             d_flags;
    std::string        d_service;
    std::string        d_operation;
    std::string        d_topic;
    Element            d_body;

  public:
    Message();
    explicit Message(MessageType::Value type, Uint64 correlationId = 0);
    // Compiler-generated copy, assignment and destructor are used.

    // MANIPULATORS
    void setType(MessageType::Value type);
    void setCorrelationId(Uint64 id);
    void setFlags(Uint32 flags);
    void addFlags(Uint32 flags);
    void setService(const std::string& service);
    void setOperation(const std::string& operation);
    void setTopic(const std::string& topic);
    Element& body();
    void reset();

    // ACCESSORS
    MessageType::Value type() const;
    Uint64 correlationId() const;
    Uint32 flags() const;
    bool hasFlag(Uint32 flag) const;
    const std::string& service() const;
    const std::string& operation() const;
    const std::string& topic() const;
    const Element& body() const;

    std::string describe() const;
        // One-line summary for logs.
};

}  // close namespace mtcore

#endif
