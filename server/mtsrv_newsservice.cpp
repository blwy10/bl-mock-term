// mtsrv_newsservice.cpp                                              -*-C++-*-
#include "mtsrv_newsservice.h"

#include <mtcore_time.h>

namespace mtsrv {

using mtcore::Element;
using mtcore::Message;
using mtcore::MessageFlags;
using mtcore::MessageType;

namespace {

const char k_ALL_TOPIC[] = "HEADLINES";

}  // close unnamed namespace

NewsService::NewsService(NewsEngine *engine)
: d_engine_p(engine)
, d_published(0)
{
}

const char *NewsService::name() const
{
    return "//mt/news";
}

const char *NewsService::description() const
{
    return "Headline archive search and real-time headlines";
}

void NewsService::handleRequest(ClientSession *session, const Message& request)
{
    if (request.operation() != "Search") {
        Service::handleRequest(session, request);
        return;
    }
    std::string query = request.body().getChildString("query");
    int max = (int)request.body().getChildInt64("maxResults", 50);
    if (max <= 0 || max > 500) {
        max = 50;
    }
    std::vector<Headline> found;
    d_engine_p->search(&found, query, (size_t)max);

    Message reply;
    ServiceUtil::initReply(&reply, MessageType::e_RESPONSE, request);
    Element *list = reply.body().addChild("headlines");
    list->setArray(true);
    for (size_t i = 0; i < found.size(); ++i) {
        found[i].toElement(list->appendItem());
    }
    session->send(reply);
}

void NewsService::handleSubscribe(ClientSession *session,
                                  const Message& subscribe)
{
    Subscription sub;
    sub.session       = session;
    sub.correlationId = subscribe.correlationId();
    sub.topic         = subscribe.topic().empty() ? std::string(k_ALL_TOPIC)
                                                  : subscribe.topic();
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

void NewsService::handleUnsubscribe(ClientSession *session,
                                    Uint64         correlationId)
{
    if (d_table.remove(NULL, session->sessionId(), correlationId)) {
        session->adjustSubscriptions(-1);
    }
}

void NewsService::sessionClosed(ClientSession *session)
{
    int n = d_table.removeSession(NULL, session->sessionId());
    session->adjustSubscriptions(-n);
}

void NewsService::publish(const Headline& headline)
{
    std::vector<Subscription> subs;
    d_table.allSubscriptions(&subs);
    Message update;
    for (std::vector<Subscription>::const_iterator it = subs.begin();
         it != subs.end();
         ++it) {
        if (it->topic != k_ALL_TOPIC && !headline.matches(it->topic)) {
            continue;
        }
        update.reset();
        update.setType(MessageType::e_SUBSCRIPTION_DATA);
        update.setCorrelationId(it->correlationId);
        update.setService(name());
        update.setTopic(it->topic);
        update.body().setArray(false);
        headline.toElement(&update.body());
        if (it->session->send(update)) {
            ::InterlockedIncrement64(&d_published);
        }
    }
}

void NewsService::appendStatistics(Element *stats) const
{
    stats->setChildInt64("subscriptions", d_table.size());
    stats->setChildInt64("published", d_published);
    stats->setChildInt64("archive", (mtcore::Int64)d_engine_p->archiveSize());
}

void NewsService::describeOperations(Element *operations) const
{
    operations->appendItem()->setString("Search");
    operations->appendItem()->setString("<subscribe: topic=HEADLINES|query>");
}

}  // close namespace mtsrv
