// mtsrv_refdataservice.cpp                                           -*-C++-*-
#include "mtsrv_refdataservice.h"

#include "mtsrv_fields.h"
#include "mtsrv_history.h"

#include <mtcore_time.h>

namespace mtsrv {

using mtcore::Element;
using mtcore::Message;
using mtcore::MessageType;

RefdataService::RefdataService(TickerPlant *plant)
: d_plant_p(plant)
, d_requests(0)
{
}

const char *RefdataService::name() const
{
    return "//mt/refdata";
}

const char *RefdataService::description() const
{
    return "Security master, reference data, history and field dictionary";
}

void RefdataService::describeSecurity(Element *element, const Security& sec)
{
    element->setChildString("ticker", sec.ticker);
    element->setChildString("name", sec.name);
    element->setChildString("assetClass",
                            AssetClass::toString(sec.assetClass));
    element->setChildString("region", sec.region);
    element->setChildString("currency", sec.currency);
    element->setChildString("exchange", sec.exchange);
    element->setChildInt64("decimals", sec.decimals);
    element->setChildFloat64("basePrice", sec.basePrice);
    if (sec.assetClass == AssetClass::e_EQUITY) {
        const Fundamentals& f = sec.fundamentals;
        Element *fe = element->addChild("fundamentals");
        fe->setChildString("sector", f.sector);
        fe->setChildFloat64("marketCapBn", f.marketCapBn);
        fe->setChildFloat64("pe", f.pe);
        fe->setChildFloat64("dividendYield", f.dividendYield);
        fe->setChildFloat64("eps", PriceUtil::round(f.eps, 2));
        fe->setChildFloat64("beta", f.beta);
        fe->setChildString("description", f.description);
    }
}

void RefdataService::handleRequest(ClientSession *session,
                                   const Message& request)
{
    ::InterlockedIncrement64(&d_requests);
    const std::string& op = request.operation();
    if (op == "SecurityList") {
        securityList(session, request);
    }
    else if (op == "SecuritySearch") {
        securitySearch(session, request);
    }
    else if (op == "ReferenceData") {
        referenceData(session, request);
    }
    else if (op == "HistoricalData") {
        historicalData(session, request);
    }
    else if (op == "FieldList") {
        fieldList(session, request);
    }
    else {
        Service::handleRequest(session, request);
    }
}

void RefdataService::securityList(ClientSession *session,
                                  const Message& request)
{
    const SecurityMaster& master = d_plant_p->master();
    Message reply;
    ServiceUtil::initReply(&reply, MessageType::e_RESPONSE, request);
    Element *list = reply.body().addChild("securities");
    list->setArray(true);
    for (int i = 0; i < master.size(); ++i) {
        describeSecurity(list->appendItem(), master.at(i));
    }
    session->send(reply);
}

void RefdataService::securitySearch(ClientSession *session,
                                    const Message& request)
{
    std::string query = request.body().getChildString("query");
    int max = (int)request.body().getChildInt64("maxResults", 10);
    std::vector<const Security *> matches;
    d_plant_p->master().search(&matches, query, max);

    Message reply;
    ServiceUtil::initReply(&reply, MessageType::e_RESPONSE, request);
    Element *list = reply.body().addChild("results");
    list->setArray(true);
    for (size_t i = 0; i < matches.size(); ++i) {
        Element *item = list->appendItem();
        item->setChildString("ticker", matches[i]->ticker);
        item->setChildString("name", matches[i]->name);
    }
    session->send(reply);
}

void RefdataService::referenceData(ClientSession *session,
                                   const Message& request)
{
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
                             false);

    Message reply;
    ServiceUtil::initReply(&reply, MessageType::e_RESPONSE, request);
    Element *data = reply.body().addChild("securityData");
    data->setArray(true);
    for (size_t i = 0; i < securities->numChildren(); ++i) {
        const std::string& ticker = securities->childAt(i)->getString();
        Element *item = data->appendItem();
        item->setChildString("security", ticker);
        const Security *sec = d_plant_p->master().find(ticker);
        if (sec == NULL) {
            ServiceUtil::fillError(item->addChild("securityError"),
                                   ErrorCode::e_UNKNOWN_SECURITY,
                                   "unknown security");
            continue;
        }
        Tick tick;
        bool haveTick = d_plant_p->cache().get(&tick, ticker);
        Element *fieldData = item->addChild("fieldData");
        fieldData->setArray(false);
        FieldMapper::populateAll(fieldData,
                                 fields,
                                 *sec,
                                 haveTick ? &tick : NULL);
    }
    Element *exceptions = reply.body().addChild("fieldExceptions");
    exceptions->setArray(true);
    for (size_t i = 0; i < invalid.size(); ++i) {
        exceptions->appendItem()->setString(invalid[i]);
    }
    session->send(reply);
}

void RefdataService::historicalData(ClientSession *session,
                                    const Message& request)
{
    std::string ticker = request.body().getChildString("security");
    std::string rangeText = request.body().getChildString("range", "1D");
    const Security *sec = d_plant_p->master().find(ticker);
    if (sec == NULL) {
        ServiceUtil::sendRequestFailure(session,
                                        request,
                                        ErrorCode::e_UNKNOWN_SECURITY,
                                        "unknown security: " + ticker);
        return;
    }
    HistoryRange::Value range;
    if (HistoryRange::fromString(&range, rangeText) != 0) {
        ServiceUtil::sendRequestFailure(session,
                                        request,
                                        ErrorCode::e_BAD_ARGUMENT,
                                        "range must be 1D, 1M or 1Y");
        return;
    }
    Tick tick;
    double last = d_plant_p->cache().get(&tick, ticker) ? tick.last
                                                        : sec->basePrice;
    Int64 now = mtcore::TimeUtil::nowMillis();
    std::vector<Bar> bars;
    HistoryGenerator::generate(&bars, *sec, range, last, now);
    int live = 0;
    if (range == HistoryRange::e_ONE_DAY) {
        std::vector<Bar> captured;
        d_plant_p->store().intradayBars(&captured,
                                        ticker,
                                        now - (Int64)bars.size() * 60000);
        live = HistoryGenerator::overlay(&bars, captured);
    }

    Message reply;
    ServiceUtil::initReply(&reply, MessageType::e_RESPONSE, request);
    Element& body = reply.body();
    body.setChildString("security", ticker);
    body.setChildString("range", HistoryRange::toString(range));
    body.setChildInt64("liveBars", live);
    Element *list = body.addChild("bars");
    list->setArray(true);
    for (std::vector<Bar>::const_iterator it = bars.begin();
         it != bars.end();
         ++it) {
        Element *b = list->appendItem();
        b->setChildDatetime("time", it->timestamp);
        b->setChildFloat64("open", it->open);
        b->setChildFloat64("high", it->high);
        b->setChildFloat64("low", it->low);
        b->setChildFloat64("close", it->close);
        b->setChildInt64("volume", it->volume);
    }
    session->send(reply);
}

void RefdataService::fieldList(ClientSession *session, const Message& request)
{
    Message reply;
    ServiceUtil::initReply(&reply, MessageType::e_RESPONSE, request);
    Element *list = reply.body().addChild("fields");
    list->setArray(true);
    for (int i = 0; i < FieldDictionary::count(); ++i) {
        const FieldDef& def = FieldDictionary::at(i);
        Element *item = list->appendItem();
        item->setChildString("mnemonic", def.mnemonic);
        item->setChildString("id", def.fieldId);
        item->setChildString("type", mtcore::DataType::toString(def.type));
        item->setChildString("category",
                             FieldCategory::toString(def.category));
        item->setChildString("description", def.description);
    }
    session->send(reply);
}

void RefdataService::appendStatistics(Element *stats) const
{
    stats->setChildInt64("requests", d_requests);
    stats->setChildInt64("securities", d_plant_p->master().size());
}

void RefdataService::describeOperations(Element *operations) const
{
    operations->appendItem()->setString("SecurityList");
    operations->appendItem()->setString("SecuritySearch");
    operations->appendItem()->setString("ReferenceData");
    operations->appendItem()->setString("HistoricalData");
    operations->appendItem()->setString("FieldList");
}

}  // close namespace mtsrv
