// mtsrv_msgservice.cpp                                               -*-C++-*-
#include "mtsrv_msgservice.h"

#include <mtcore_stringutil.h>
#include <mtcore_time.h>

namespace mtsrv {

using mtcore::Element;
using mtcore::Message;
using mtcore::MessageType;
using mtcore::StringUtil;

MsgService::MsgService(ChatEngine *engine)
: d_engine_p(engine)
, d_delivered(0)
{
}

const char *MsgService::name() const
{
    return "//mt/msg";
}

const char *MsgService::description() const
{
    return "Instant messaging between users and simulated counterparties";
}

void MsgService::handleRequest(ClientSession *session, const Message& request)
{
    if (request.operation() != "Send") {
        Service::handleRequest(session, request);
        return;
    }
    std::string from = StringUtil::trim(request.body().getChildString("from"));
    std::string to   = StringUtil::trim(request.body().getChildString("to"));
    std::string text = request.body().getChildString("text");
    if (from.empty()) {
        from = session->user();
    }
    if (to.empty() || StringUtil::trim(text).empty()) {
        ServiceUtil::sendRequestFailure(session,
                                        request,
                                        ErrorCode::e_BAD_ARGUMENT,
                                        "'to' and 'text' are required");
        return;
    }
    ChatMessage sent = d_engine_p->send(from,
                                        to,
                                        text,
                                        mtcore::TimeUtil::nowMillis());
    Message reply;
    ServiceUtil::initReply(&reply, MessageType::e_RESPONSE, request);
    sent.toElement(reply.body().addChild("message"), from);
    session->send(reply);
    deliver(sent);
}

void MsgService::handleSubscribe(ClientSession *session,
                                 const Message& subscribe)
{
    if (StringUtil::trim(subscribe.topic()).empty()) {
        ServiceUtil::sendSubscriptionFailure(session,
                                             subscribe,
                                             ErrorCode::e_BAD_ARGUMENT,
                                             "topic must be a user name");
        return;
    }
    Subscription sub;
    sub.session       = session;
    sub.correlationId = subscribe.correlationId();
    sub.topic         = StringUtil::toUpper(subscribe.topic());
    sub.createdMs     = mtcore::TimeUtil::nowMillis();
    if (d_table.add(sub) != 0) {
        ServiceUtil::sendSubscriptionFailure(session,
                                             subscribe,
                                             ErrorCode::e_DUPLICATE_ID,
                                             "correlation id already in use");
        return;
    }
    session->adjustSubscriptions(1);
    Message started;
    ServiceUtil::initReply(&started,
                           MessageType::e_SUBSCRIPTION_STARTED,
                           subscribe);
    session->send(started);
}

void MsgService::handleUnsubscribe(ClientSession *session,
                                   Uint64         correlationId)
{
    if (d_table.remove(NULL, session->sessionId(), correlationId)) {
        session->adjustSubscriptions(-1);
    }
}

void MsgService::sessionClosed(ClientSession *session)
{
    int n = d_table.removeSession(NULL, session->sessionId());
    session->adjustSubscriptions(-n);
}

void MsgService::deliver(const ChatMessage& message)
{
    std::vector<Subscription> subs;
    d_table.subscribersOf(&subs, StringUtil::toUpper(message.to));
    Message update;
    for (std::vector<Subscription>::const_iterator it = subs.begin();
         it != subs.end();
         ++it) {
        update.reset();
        update.setType(MessageType::e_SUBSCRIPTION_DATA);
        update.setCorrelationId(it->correlationId);
        update.setService(name());
        update.setTopic(it->topic);
        update.body().setArray(false);
        message.toElement(&update.body(), message.to);
        if (it->session->send(update)) {
            ::InterlockedIncrement64(&d_delivered);
        }
    }
}

void MsgService::appendStatistics(Element *stats) const
{
    stats->setChildInt64("subscriptions", d_table.size());
    stats->setChildInt64("delivered", d_delivered);
    stats->setChildInt64("sent", (mtcore::Int64)d_engine_p->sentCount());
}

void MsgService::describeOperations(Element *operations) const
{
    operations->appendItem()->setString("Send");
    operations->appendItem()->setString("<subscribe: topic=user>");
}

}  // close namespace mtsrv
