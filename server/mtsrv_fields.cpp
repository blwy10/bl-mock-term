// mtsrv_fields.cpp                                                   -*-C++-*-
#include "mtsrv_fields.h"

#include "mtsrv_security.h"
#include "mtsrv_tick.h"

#include <string.h>

namespace mtsrv {

using mtcore::DataType;
using mtcore::Element;

namespace {

const FieldDef k_FIELDS[] = {
  // mnemonic          id       type                 category
  { "LAST_PRICE",      "RT001", DataType::e_FLOAT64, FieldCategory::e_REALTIME,
    "Last traded price" },
  { "BID",             "RT002", DataType::e_FLOAT64, FieldCategory::e_REALTIME,
    "Best bid" },
  { "ASK",             "RT003", DataType::e_FLOAT64, FieldCategory::e_REALTIME,
    "Best ask" },
  { "OPEN",            "RT004", DataType::e_FLOAT64, FieldCategory::e_REALTIME,
    "Session open" },
  { "HIGH",            "RT005", DataType::e_FLOAT64, FieldCategory::e_REALTIME,
    "Session high" },
  { "LOW",             "RT006", DataType::e_FLOAT64, FieldCategory::e_REALTIME,
    "Session low" },
  { "PREV_CLOSE",      "RT007", DataType::e_FLOAT64, FieldCategory::e_REALTIME,
    "Previous session close" },
  { "NET_CHANGE",      "RT008", DataType::e_FLOAT64, FieldCategory::e_REALTIME,
    "Change from previous close" },
  { "PCT_CHANGE",      "RT009", DataType::e_FLOAT64, FieldCategory::e_REALTIME,
    "Percent change from previous close" },
  { "VOLUME",          "RT010", DataType::e_INT64,   FieldCategory::e_REALTIME,
    "Session volume" },
  { "LAST_TRADE_SIZE", "RT011", DataType::e_INT64,   FieldCategory::e_REALTIME,
    "Size of the last trade" },
  { "LAST_UPDATE_TIME","RT012", DataType::e_DATETIME,FieldCategory::e_REALTIME,
    "Time of the last update" },
  { "SEQUENCE_NUMBER", "RT013", DataType::e_INT64,   FieldCategory::e_REALTIME,
    "Ticker plant sequence number" },
  { "FEED_SOURCE",     "RT014", DataType::e_STRING,  FieldCategory::e_REALTIME,
    "Originating feed" },

  { "TICKER",          "ST001", DataType::e_STRING,  FieldCategory::e_STATIC,
    "Security identifier" },
  { "NAME",            "ST002", DataType::e_STRING,  FieldCategory::e_STATIC,
    "Security name" },
  { "ASSET_CLASS",     "ST003", DataType::e_STRING,  FieldCategory::e_STATIC,
    "Asset class (Equity, Curncy, Index, Rate)" },
  { "REGION",          "ST004", DataType::e_STRING,  FieldCategory::e_STATIC,
    "Region" },
  { "CURRENCY",        "ST005", DataType::e_STRING,  FieldCategory::e_STATIC,
    "Quote currency" },
  { "EXCHANGE",        "ST006", DataType::e_STRING,  FieldCategory::e_STATIC,
    "Primary venue / feed code" },
  { "PRICE_DECIMALS",  "ST007", DataType::e_INT64,   FieldCategory::e_STATIC,
    "Display decimals" },
  { "SECTOR",          "ST008", DataType::e_STRING,  FieldCategory::e_STATIC,
    "Industry sector (equities)" },
  { "MARKET_CAP_BN",   "ST009", DataType::e_FLOAT64, FieldCategory::e_STATIC,
    "Market capitalisation, billions" },
  { "PE_RATIO",        "ST010", DataType::e_FLOAT64, FieldCategory::e_STATIC,
    "Price / earnings" },
  { "DIVIDEND_YIELD",  "ST011", DataType::e_FLOAT64, FieldCategory::e_STATIC,
    "Trailing dividend yield, percent" },
  { "EPS",             "ST012", DataType::e_FLOAT64, FieldCategory::e_STATIC,
    "Earnings per share" },
  { "BETA",            "ST013", DataType::e_FLOAT64, FieldCategory::e_STATIC,
    "Beta to the local market" },
  { "DESCRIPTION",     "ST014", DataType::e_STRING,  FieldCategory::e_STATIC,
    "Business description" }
};

const int k_NUM_FIELDS = (int)MTCORE_ARRAY_SIZE(k_FIELDS);

}  // close unnamed namespace

const char *FieldCategory::toString(Value value)
{
    return value == e_REALTIME ? "RealTime" : "Static";
}

int FieldDictionary::count()
{
    return k_NUM_FIELDS;
}

const FieldDef& FieldDictionary::at(int index)
{
    return k_FIELDS[index];
}

const FieldDef *FieldDictionary::lookup(const std::string& mnemonic)
{
    for (int i = 0; i < k_NUM_FIELDS; ++i) {
        if (_stricmp(k_FIELDS[i].mnemonic, mnemonic.c_str()) == 0
         || _stricmp(k_FIELDS[i].fieldId, mnemonic.c_str()) == 0) {
            return &k_FIELDS[i];
        }
    }
    return NULL;
}

void FieldDictionary::realtimeFields(std::vector<const FieldDef *> *result)
{
    for (int i = 0; i < k_NUM_FIELDS; ++i) {
        if (k_FIELDS[i].category == FieldCategory::e_REALTIME) {
            result->push_back(&k_FIELDS[i]);
        }
    }
}

void FieldDictionary::staticFields(std::vector<const FieldDef *> *result)
{
    for (int i = 0; i < k_NUM_FIELDS; ++i) {
        if (k_FIELDS[i].category == FieldCategory::e_STATIC) {
            result->push_back(&k_FIELDS[i]);
        }
    }
}

void FieldDictionary::resolve(std::vector<const FieldDef *> *result,
                              std::vector<std::string>      *invalid,
                              const Element                 *fieldArray,
                              bool                           defaultRealtime)
{
    if (fieldArray && fieldArray->numChildren() > 0) {
        for (size_t i = 0; i < fieldArray->numChildren(); ++i) {
            const std::string& name = fieldArray->childAt(i)->getString();
            const FieldDef *def = lookup(name);
            if (def) {
                result->push_back(def);
            }
            else {
                invalid->push_back(name);
            }
        }
        return;
    }
    if (defaultRealtime) {
        realtimeFields(result);
    }
    else {
        for (int i = 0; i < k_NUM_FIELDS; ++i) {
            result->push_back(&k_FIELDS[i]);
        }
    }
}

                            // -----------------
                            // struct FieldMapper
                            // -----------------

bool FieldMapper::populate(Element         *parent,
                           const FieldDef&  field,
                           const Security&  security,
                           const Tick      *tick)
{
    const char *m = field.mnemonic;
    if (field.category == FieldCategory::e_REALTIME) {
        if (tick == NULL) {
            return false;
        }
        int dp = security.decimals;
        if      (!strcmp(m, "LAST_PRICE")) parent->setChildFloat64(m, tick->last);
        else if (!strcmp(m, "BID"))        parent->setChildFloat64(m, tick->bid);
        else if (!strcmp(m, "ASK"))        parent->setChildFloat64(m, tick->ask);
        else if (!strcmp(m, "OPEN"))       parent->setChildFloat64(m, tick->open);
        else if (!strcmp(m, "HIGH"))       parent->setChildFloat64(m, tick->high);
        else if (!strcmp(m, "LOW"))        parent->setChildFloat64(m, tick->low);
        else if (!strcmp(m, "PREV_CLOSE")) {
            parent->setChildFloat64(m, tick->prevClose);
        }
        else if (!strcmp(m, "NET_CHANGE")) {
            parent->setChildFloat64(m, PriceUtil::round(tick->change, dp));
        }
        else if (!strcmp(m, "PCT_CHANGE")) {
            parent->setChildFloat64(m, PriceUtil::round(tick->changePct, 4));
        }
        else if (!strcmp(m, "VOLUME"))     parent->setChildInt64(m, tick->volume);
        else if (!strcmp(m, "LAST_TRADE_SIZE")) {
            parent->setChildInt64(m, tick->lastSize);
        }
        else if (!strcmp(m, "LAST_UPDATE_TIME")) {
            parent->setChildDatetime(m, tick->timestamp);
        }
        else if (!strcmp(m, "SEQUENCE_NUMBER")) {
            parent->setChildInt64(m, (mtcore::Int64)tick->sequence);
        }
        else if (!strcmp(m, "FEED_SOURCE")) parent->setChildString(m, tick->feed);
        else return false;
        return true;
    }

    const Fundamentals& f = security.fundamentals;
    bool isEquity = security.assetClass == AssetClass::e_EQUITY;
    if      (!strcmp(m, "TICKER"))   parent->setChildString(m, security.ticker);
    else if (!strcmp(m, "NAME"))     parent->setChildString(m, security.name);
    else if (!strcmp(m, "ASSET_CLASS")) {
        parent->setChildString(m, AssetClass::toString(security.assetClass));
    }
    else if (!strcmp(m, "REGION"))   parent->setChildString(m, security.region);
    else if (!strcmp(m, "CURRENCY")) parent->setChildString(m, security.currency);
    else if (!strcmp(m, "EXCHANGE")) parent->setChildString(m, security.exchange);
    else if (!strcmp(m, "PRICE_DECIMALS")) {
        parent->setChildInt64(m, security.decimals);
    }
    else if (!isEquity) {
        return false;
    }
    else if (!strcmp(m, "SECTOR"))        parent->setChildString(m, f.sector);
    else if (!strcmp(m, "MARKET_CAP_BN")) parent->setChildFloat64(m, f.marketCapBn);
    else if (!strcmp(m, "PE_RATIO"))      parent->setChildFloat64(m, f.pe);
    else if (!strcmp(m, "DIVIDEND_YIELD")) {
        parent->setChildFloat64(m, f.dividendYield);
    }
    else if (!strcmp(m, "EPS")) {
        parent->setChildFloat64(m, PriceUtil::round(f.eps, 2));
    }
    else if (!strcmp(m, "BETA"))        parent->setChildFloat64(m, f.beta);
    else if (!strcmp(m, "DESCRIPTION")) parent->setChildString(m, f.description);
    else return false;
    return true;
}

void FieldMapper::populateAll(Element                              *parent,
                              const std::vector<const FieldDef *>&  fields,
                              const Security&                       security,
                              const Tick                           *tick)
{
    for (std::vector<const FieldDef *>::const_iterator it = fields.begin();
         it != fields.end();
         ++it) {
        populate(parent, **it, security, tick);
    }
}

}  // close namespace mtsrv
