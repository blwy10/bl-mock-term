// mtsrv_mktdataservice.cpp                                           -*-C++-*-
#include "mtsrv_mktdataservice.h"

#include <mtcore_log.h>
#include <mtcore_stringutil.h>
#include <mtcore_time.h>

namespace mtsrv {

using mtcore::Element;
using mtcore::Message;
using mtcore::MessageFlags;
using mtcore::MessageType;
using mtcore::StringUtil;

MktdataService::MktdataService(TickerPlant *plant)
: d_plant_p(plant)
, d_updatesSent(0)
, d_ticksSeen(0)
{
    d_plant_p->addListener(this);
}

MktdataService::~MktdataService()
{
    d_plant_p->removeListener(this);
}

const char *MktdataService::name() const
{
    return "//mt/mktdata";
}

const char *MktdataService::description() const
{
    return "Real-time top-of-book and session statistics";
}

void MktdataService::buildUpdate(Message             *message,
                                 const Subscription&  subscription,
                                 const Security&      security,
                                 const Tick&          tick,
                                 mtcore::Uint32       flags) const
{
    message->reset();
    message->setType(MessageType::e_SUBSCRIPTION_DATA);
    message->setCorrelationId(subscription.correlationId);
    message->setService(name());
    message->setTopic(subscription.topic);
    message->setFlags(flags);
    Element& body = message->body();
    body.setArray(false);
    FieldMapper::populateAll(&body, subscription.fields, security, &tick);
}

void MktdataService::handleSubscribe(ClientSession *session,
                                     const Message& subscribe)
{
    const Security *sec = d_plant_p->master().find(subscribe.topic());
    if (sec == NULL) {
        ServiceUtil::sendSubscriptionFailure(session,
                                             subscribe,
                                             ErrorCode::e_UNKNOWN_SECURITY,
                                             "unknown security: "
                                                 + subscribe.topic());
        return;
    }
    Subscription sub;
    std::vector<std::string> invalid;
    FieldDictionary::resolve(&sub.fields,
                             &invalid,
                             subscribe.body().getChild("fields"),
                             true);
    if (!invalid.empty()) {
        ServiceUtil::sendSubscriptionFailure(
                                session,
                                subscribe,
                                ErrorCode::e_UNKNOWN_FIELD,
                                "unknown field(s): "
                                    + StringUtil::join(invalid, ", "));
        return;
    }
    sub.session       = session;
    sub.correlationId = subscribe.correlationId();
    sub.topic         = subscribe.topic();
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

    Tick tick;
    if (d_plant_p->cache().get(&tick, sec->ticker)) {
        Message image;
        buildUpdate(&image, sub, *sec, tick, MessageFlags::e_INITIAL_PAINT);
        session->send(image);
    }
}

void MktdataService::handleUnsubscribe(ClientSession *session,
                                       Uint64         correlationId)
{
    if (d_table.remove(NULL, session->sessionId(), correlationId)) {
        session->adjustSubscriptions(-1);
    }
}

void MktdataService::sessionClosed(ClientSession *session)
{
    int n = d_table.removeSession(NULL, session->sessionId());
    if (n > 0) {
        session->adjustSubscriptions(-n);
        MT_LOG_INFO("mktdata: dropped %d subscriptions for session %I64u",
                    n,
                    session->sessionId());
    }
}

void MktdataService::onTick(const Tick& tick)
{
    ::InterlockedIncrement64(&d_ticksSeen);
    std::vector<Subscription> subs;
    d_table.subscribersOf(&subs, tick.ticker);
    if (subs.empty()) {
        return;
    }
    const Security *sec = d_plant_p->master().find(tick.ticker);
    if (sec == NULL) {
        return;
    }
    Message update;
    for (std::vector<Subscription>::const_iterator it = subs.begin();
         it != subs.end();
         ++it) {
        buildUpdate(&update, *it, *sec, tick, MessageFlags::e_NONE);
        if (it->session->send(update)) {
            ::InterlockedIncrement64(&d_updatesSent);
        }
    }
}

void MktdataService::handleRequest(ClientSession *session,
                                   const Message& request)
{
    if (request.operation() != "Snapshot") {
        Service::handleRequest(session, request);
        return;
    }
    const Element *securities = request.body().getChild("securities");
    if (securities == NULL || !securities->isArray()) {
        ServiceUtil::sendRequestFailure(session,
                                        request,
                                        ErrorCode::e_BAD_ARGUMENT,
                                        "'securities' array is required");
        return;
    }
    std::vector<const FieldDef *> fields;
    std::vector<std::string> invalid;
    FieldDictionary::resolve(&fields,
                             &invalid,
                             request.body().getChild("fields"),
                             true);
    Message reply;
    ServiceUtil::initReply(&reply, MessageType::e_RESPONSE, request);
    Element *data = reply.body().addChild("securityData");
    data->setArray(true);
    for (size_t i = 0; i < securities->numChildren(); ++i) {
        const std::string& ticker = securities->childAt(i)->getString();
        Element *item = data->appendItem();
        item->setChildString("security", ticker);
        const Security *sec = d_plant_p->master().find(ticker);
        Tick tick;
        if (sec == NULL || !d_plant_p->cache().get(&tick, ticker)) {
            ServiceUtil::fillError(item->addChild("securityError"),
                                   ErrorCode::e_UNKNOWN_SECURITY,
                                   "unknown security");
            continue;
        }
        Element *fieldData = item->addChild("fieldData");
        fieldData->setArray(false);
        FieldMapper::populateAll(fieldData, fields, *sec, &tick);
    }
    Element *exceptions = reply.body().addChild("fieldExceptions");
    exceptions->setArray(true);
    for (size_t i = 0; i < invalid.size(); ++i) {
        exceptions->appendItem()->setString(invalid[i]);
    }
    session->send(reply);
}

void MktdataService::appendStatistics(Element *stats) const
{
    stats->setChildInt64("subscriptions", d_table.size());
    stats->setChildInt64("topics", d_table.topicCount());
    stats->setChildInt64("ticksSeen", d_ticksSeen);
    stats->setChildInt64("updatesSent", d_updatesSent);
}

void MktdataService::describeOperations(Element *operations) const
{
    operations->appendItem()->setString("Snapshot");
    operations->appendItem()->setString("<subscribe: topic=security>");
}

const SubscriptionTable& MktdataService::table() const
{
    return d_table;
}

}  // close namespace mtsrv
