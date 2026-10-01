// mtsrv_service.cpp                                                  -*-C++-*-
#include "mtsrv_service.h"

#include <mtcore_log.h>

namespace mtsrv {

using mtcore::Element;
using mtcore::Message;
using mtcore::MessageType;

Service::~Service()
{
}

void Service::handleRequest(ClientSession *session, const Message& request)
{
    ServiceUtil::sendRequestFailure(session,
                                    request,
                                    ErrorCode::e_UNKNOWN_OPERATION,
                                    "operation not supported: "
                                        + request.operation());
}

void Service::handleSubscribe(ClientSession *session, const Message& subscribe)
{
    ServiceUtil::sendSubscriptionFailure(session,
                                         subscribe,
                                         ErrorCode::e_UNKNOWN_OPERATION,
                                         "service does not support "
                                         "subscriptions");
}

void Service::handleUnsubscribe(ClientSession *, Uint64)
{
}

void Service::sessionClosed(ClientSession *)
{
}

void Service::appendStatistics(Element *) const
{
}

void Service::describeOperations(Element *) const
{
}

const char *ErrorCode::category(Value value)
{
    switch (value) {
      case e_UNKNOWN_SERVICE:   return "UNKNOWN_SERVICE";
      case e_UNKNOWN_OPERATION: return "UNKNOWN_OPERATION";
      case e_BAD_ARGUMENT:      return "BAD_ARGUMENT";
      case e_UNKNOWN_SECURITY:  return "UNKNOWN_SECURITY";
      case e_UNKNOWN_FIELD:     return "UNKNOWN_FIELD";
      case e_NOT_LOGGED_ON:     return "NOT_LOGGED_ON";
      case e_DUPLICATE_ID:      return "DUPLICATE_ID";
      case e_INTERNAL:          return "INTERNAL";
    }
    return "UNKNOWN";
}

void ServiceUtil::initReply(Message               *reply,
                            MessageType::Value     type,
                            const Message&         original)
{
    reply->reset();
    reply->setType(type);
    reply->setCorrelationId(original.correlationId());
    reply->setService(original.service());
    reply->setOperation(original.operation());
    reply->setTopic(original.topic());
}

void ServiceUtil::fillError(Element           *reason,
                            ErrorCode::Value   code,
                            const std::string& text)
{
    reason->setChildInt64("code", code);
    reason->setChildString("category", ErrorCode::category(code));
    reason->setChildString("message", text);
}

void ServiceUtil::sendRequestFailure(ClientSession     *session,
                                     const Message&     request,
                                     ErrorCode::Value   code,
                                     const std::string& text)
{
    MT_LOG_DEBUG("request failure cid=%I64u: %s",
                 request.correlationId(),
                 text.c_str());
    Message reply;
    initReply(&reply, MessageType::e_REQUEST_FAILURE, request);
    fillError(reply.body().addChild("reason"), code, text);
    session->send(reply);
}

void ServiceUtil::sendSubscriptionFailure(ClientSession     *session,
                                          const Message&     subscribe,
                                          ErrorCode::Value   code,
                                          const std::string& text)
{
    Message reply;
    initReply(&reply, MessageType::e_SUBSCRIPTION_FAILURE, subscribe);
    fillError(reply.body().addChild("reason"), code, text);
    session->send(reply);
}

}  // close namespace mtsrv
