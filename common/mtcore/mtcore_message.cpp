// mtcore_message.cpp                                                 -*-C++-*-
#include "mtcore_message.h"

#include "mtcore_stringutil.h"

#include <string.h>

namespace mtcore {

const char *MessageType::toString(Value value)
{
    switch (value) {
      case e_UNKNOWN:                 return "UNKNOWN";
      case e_LOGON:                   return "LOGON";
      case e_LOGON_ACK:               return "LOGON_ACK";
      case e_LOGOFF:                  return "LOGOFF";
      case e_HEARTBEAT:               return "HEARTBEAT";
      case e_HEARTBEAT_ACK:           return "HEARTBEAT_ACK";
      case e_REQUEST:                 return "REQUEST";
      case e_RESPONSE:                return "RESPONSE";
      case e_PARTIAL_RESPONSE:        return "PARTIAL_RESPONSE";
      case e_REQUEST_FAILURE:         return "REQUEST_FAILURE";
      case e_SUBSCRIBE:               return "SUBSCRIBE";
      case e_UNSUBSCRIBE:             return "UNSUBSCRIBE";
      case e_SUBSCRIPTION_STARTED:    return "SUBSCRIPTION_STARTED";
      case e_SUBSCRIPTION_FAILURE:    return "SUBSCRIPTION_FAILURE";
      case e_SUBSCRIPTION_DATA:       return "SUBSCRIPTION_DATA";
      case e_SUBSCRIPTION_TERMINATED: return "SUBSCRIPTION_TERMINATED";
      case e_SESSION_STATUS:          return "SESSION_STATUS";
      case e_SERVICE_STATUS:          return "SERVICE_STATUS";
    }
    return "???";
}

bool MessageType::isValid(int value)
{
    return value != e_UNKNOWN && strcmp(toString((Value)value), "???") != 0;
}

Message::Message()
: d_type(MessageType::e_UNKNOWN)
, d_correlationId(0)
, d_flags(0)
, d_body("body")
{
}

Message::Message(MessageType::Value type, Uint64 correlationId)
: d_type(type)
, d_correlationId(correlationId)
, d_flags(0)
, d_body("body")
{
}

void Message::setType(MessageType::Value type)
{
    d_type = type;
}

void Message::setCorrelationId(Uint64 id)
{
    d_correlationId = id;
}

void Message::setFlags(Uint32 flags)
{
    d_flags = flags;
}

void Message::addFlags(Uint32 flags)
{
    d_flags |= flags;
}

void Message::setService(const std::string& service)
{
    d_service = service;
}

void Message::setOperation(const std::string& operation)
{
    d_operation = operation;
}

void Message::setTopic(const std::string& topic)
{
    d_topic = topic;
}

Element& Message::body()
{
    return d_body;
}

void Message::reset()
{
    d_type          = MessageType::e_UNKNOWN;
    d_correlationId = 0;
    d_flags         = 0;
    d_service.clear();
    d_operation.clear();
    d_topic.clear();
    d_body.clear();
}

MessageType::Value Message::type() const
{
    return d_type;
}

Uint64 Message::correlationId() const
{
    return d_correlationId;
}

Uint32 Message::flags() const
{
    return d_flags;
}

bool Message::hasFlag(Uint32 flag) const
{
    return (d_flags & flag) != 0;
}

const std::string& Message::service() const
{
    return d_service;
}

const std::string& Message::operation() const
{
    return d_operation;
}

const std::string& Message::topic() const
{
    return d_topic;
}

const Element& Message::body() const
{
    return d_body;
}

std::string Message::describe() const
{
    return StringUtil::format("%s cid=%I64u svc=%s op=%s topic=%s flags=%u",
                              MessageType::toString(d_type),
                              d_correlationId,
                              d_service.c_str(),
                              d_operation.c_str(),
                              d_topic.c_str(),
                              d_flags);
}

}  // close namespace mtcore
