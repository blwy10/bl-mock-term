// mtsrv_newsengine.cpp                                               -*-C++-*-
#include "mtsrv_newsengine.h"

#include <mtcore_stringutil.h>

#include <math.h>
#include <stdio.h>

namespace mtsrv {

using mtcore::LockGuard;
using mtcore::StringUtil;

namespace {

const char *k_SOURCES[] = {
    "MTN Wire", "MockTerm First Word", "Desk Notes", "Street Ticker",
    "Global Briefs"
};

const char *k_EQUITY_TEMPLATES[] = {
    "%s shares %s %.1f%% as traders weigh guidance update",
    "%s announces $%d billion buyback; stock %s %.1f%%",
    "Analyst at Mock Securities %s %s to %s, sees %.0f%% upside",
    "%s said to explore strategic options for unit, people familiar say",
    "%s CEO says demand remains robust in mock earnings call",
    "Options traders pile into %s calls ahead of product event"
};

const char *k_MACRO_TEMPLATES[] = {
    "Central bank official signals patience on rate path; %s at %s",
    "%s %s as risk appetite %s across regions",
    "Mock CPI print comes in %s than forecast; %s reacts",
    "Treasury auction draws %s demand; %s moves to %s",
    "Regional PMI %s to %.1f; %s %s"
};

}  // close unnamed namespace

                                // --------------
                                // class Headline
                                // --------------

Headline::Headline()
: id(0)
, timestamp(0)
{
}

void Headline::toElement(mtcore::Element *element) const
{
    element->setChildInt64("id", id);
    element->setChildDatetime("time", timestamp);
    element->setChildString("source", source);
    element->setChildString("category", category);
    mtcore::Element *list = element->addChild("tickers");
    list->setArray(true);
    for (std::vector<std::string>::const_iterator it = tickers.begin();
         it != tickers.end();
         ++it) {
        list->appendItem()->setString(*it);
    }
    element->setChildString("headline", headline);
    element->setChildString("body", body);
}

bool Headline::matches(const std::string& query) const
{
    if (query.empty()) {
        return true;
    }
    std::string hay = headline + " " + body + " " + source + " " + category;
    for (size_t i = 0; i < tickers.size(); ++i) {
        hay += " " + tickers[i];
    }
    return StringUtil::containsNoCase(hay, query);
}

                                // ----------------
                                // class NewsEngine
                                // ----------------

NewsEngine::NewsEngine(const SecurityMaster *master,
                       const TickCache      *cache,
                       Uint64                seed,
                       size_t                capacity)
: d_master_p(master)
, d_cache_p(cache)
, d_rng(seed)
, d_capacity(capacity)
, d_nextId(1)
, d_nextDueMs(0)
, d_minIntervalMs(2500)
, d_maxIntervalMs(6000)
{
}

void NewsEngine::setInterval(int minMs, int maxMs)
{
    LockGuard guard(&d_mutex);
    d_minIntervalMs = minMs;
    d_maxIntervalMs = maxMs < minMs ? minMs : maxMs;
}

bool NewsEngine::poll(Headline *result, Int64 nowMs)
{
    {
        LockGuard guard(&d_mutex);
        if (d_nextDueMs == 0) {
            d_nextDueMs = nowMs + d_rng.range(d_minIntervalMs,
                                              d_maxIntervalMs);
            return false;
        }
        if (nowMs < d_nextDueMs) {
            return false;
        }
        d_nextDueMs = nowMs + d_rng.range(d_minIntervalMs, d_maxIntervalMs);
    }
    *result = generate(nowMs);
    return true;
}

void NewsEngine::backfill(int count, Int64 nowMs)
{
    for (int i = count; i > 0; --i) {
        generate(nowMs - (Int64)i * 180000);
    }
}

void NewsEngine::search(std::vector<Headline> *result,
                        const std::string&     query,
                        size_t                 limit) const
{
    std::string q = StringUtil::trim(query);
    LockGuard guard(&d_mutex);
    for (Archive::const_reverse_iterator it = d_archive.rbegin();
         it != d_archive.rend() && result->size() < limit;
         ++it) {
        if (it->matches(q)) {
            result->push_back(*it);
        }
    }
}

size_t NewsEngine::archiveSize() const
{
    LockGuard guard(&d_mutex);
    return d_archive.size();
}

Headline NewsEngine::generate(Int64 nowMs)
{
    LockGuard guard(&d_mutex);
#define MTSRV_PICK(ARRAY) \
    ARRAY[d_rng.range(0, (int)MTCORE_ARRAY_SIZE(ARRAY) - 1)]

    int n = d_master_p->size();
    const Security *sec = &d_master_p->at(d_rng.range(0, n - 1));
    if (sec->assetClass == AssetClass::e_EQUITY && !d_rng.chance(0.85)) {
        for (int i = 0; i < 10 && sec->assetClass == AssetClass::e_EQUITY;
             ++i) {
            sec = &d_master_p->at(d_rng.range(0, n - 1));
        }
    }
    Tick tick;
    bool haveTick = d_cache_p->get(&tick, sec->ticker);
    double chg  = haveTick ? tick.changePct : 0;
    double last = haveTick ? tick.last : sec->basePrice;
    const char *dir  = chg >= 0 ? "rise" : "fall";
    const char *dirs = chg >= 0 ? "rises" : "falls";
    std::string sym = sec->symbol();
    char level[64];
    _snprintf(level, sizeof(level), "%.*f", sec->decimals, last);

    char buf[512];
    Headline h;
    h.id        = d_nextId++;
    h.timestamp = nowMs;
    h.source    = MTSRV_PICK(k_SOURCES);
    h.tickers.push_back(sec->ticker);

    if (sec->assetClass == AssetClass::e_EQUITY) {
        h.category = "Company";
        int which = d_rng.range(0,
                                (int)MTCORE_ARRAY_SIZE(k_EQUITY_TEMPLATES) - 1);
        switch (which) {
          case 0:
            _snprintf(buf, sizeof(buf), k_EQUITY_TEMPLATES[0],
                      sym.c_str(), dir, fabs(chg));
            break;
          case 1:
            _snprintf(buf, sizeof(buf), k_EQUITY_TEMPLATES[1],
                      sym.c_str(), d_rng.range(5, 55), dirs, fabs(chg));
            break;
          case 2: {
            bool up = d_rng.chance(0.5);
            _snprintf(buf, sizeof(buf), k_EQUITY_TEMPLATES[2],
                      up ? "upgrades" : "downgrades", sym.c_str(),
                      up ? "Buy" : "Hold", 5 + d_rng.uniform() * 25);
          } break;
          default:
            _snprintf(buf, sizeof(buf), k_EQUITY_TEMPLATES[which],
                      sym.c_str());
            break;
        }
    }
    else {
        h.category = sec->assetClass == AssetClass::e_CURRENCY ? "FX"
                   : sec->assetClass == AssetClass::e_RATE     ? "Rates"
                   :                                             "Markets";
        int which = d_rng.range(0,
                                (int)MTCORE_ARRAY_SIZE(k_MACRO_TEMPLATES) - 1);
        switch (which) {
          case 0:
            _snprintf(buf, sizeof(buf), k_MACRO_TEMPLATES[0],
                      sym.c_str(), level);
            break;
          case 1:
            _snprintf(buf, sizeof(buf), k_MACRO_TEMPLATES[1],
                      sec->name.c_str(), dirs,
                      chg >= 0 ? "improves" : "fades");
            break;
          case 2:
            _snprintf(buf, sizeof(buf), k_MACRO_TEMPLATES[2],
                      d_rng.chance(0.5) ? "hotter" : "cooler", sym.c_str());
            break;
          case 3:
            _snprintf(buf, sizeof(buf), k_MACRO_TEMPLATES[3],
                      d_rng.chance(0.5) ? "strong" : "soft", sym.c_str(),
                      level);
            break;
          default:
            _snprintf(buf, sizeof(buf), k_MACRO_TEMPLATES[4],
                      d_rng.chance(0.5) ? "rises" : "slips",
                      45 + d_rng.uniform() * 10, sec->name.c_str(), dirs);
            break;
        }
    }
    buf[sizeof(buf) - 1] = 0;
    h.headline = buf;

    _snprintf(buf,
              sizeof(buf),
              "(%s) -- %s. %s (%s) last traded at %s, %+.2f%% on the "
              "session. This is simulated news generated by MockTerm for "
              "demonstration purposes only; no real market information is "
              "implied.",
              h.source.c_str(), h.headline.c_str(), sec->name.c_str(),
              sec->ticker.c_str(), level, chg);
    buf[sizeof(buf) - 1] = 0;
    h.body = buf;

    d_archive.push_back(h);
    while (d_archive.size() > d_capacity) {
        d_archive.pop_front();
    }
#undef MTSRV_PICK
    return h;
}

}  // close namespace mtsrv
