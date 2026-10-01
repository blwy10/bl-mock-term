// mtsrv_tickcache.cpp                                                -*-C++-*-
#include "mtsrv_tickcache.h"

namespace mtsrv {

using mtcore::LockGuard;

TickCache::TickCache()
: d_sequence(0)
{
}

void TickCache::initialize(const Security& security,
                           double          openPrice,
                           Int64           openingVolume,
                           Int64           nowMs)
{
    int dp = security.decimals;
    Tick t;
    t.ticker    = security.ticker;
    t.feed      = security.exchange;
    t.prevClose = PriceUtil::round(security.basePrice, dp);
    t.last      = PriceUtil::round(openPrice, dp);
    t.open      = t.last;
    t.high      = t.last;
    t.low       = t.last;
    double halfSpread = t.last * security.spreadBps / 20000.0;
    if (halfSpread < PriceUtil::tickSize(dp)) {
        halfSpread = PriceUtil::tickSize(dp);
    }
    t.bid       = PriceUtil::round(t.last - halfSpread, dp);
    t.ask       = PriceUtil::round(t.last + halfSpread, dp);
    t.change    = t.last - t.prevClose;
    t.changePct = t.prevClose != 0 ? t.change / t.prevClose * 100.0 : 0;
    t.volume    = openingVolume;
    t.timestamp = nowMs;

    LockGuard guard(&d_mutex);
    t.sequence = ++d_sequence;
    d_ticks[security.ticker] = t;
}

bool TickCache::apply(Tick             *result,
                      const FeedPacket& packet,
                      const Security&   security)
{
    LockGuard guard(&d_mutex);
    TickMap::iterator it = d_ticks.find(packet.ticker);
    if (it == d_ticks.end()) {
        return false;
    }
    Tick& t   = it->second;
    t.feed    = packet.feed;
    t.bid     = packet.bid;
    t.ask     = packet.ask;
    t.lastSize = 0;
    if (packet.kind == FeedPacket::e_TRADE) {
        t.last = packet.price;
        if (t.last > t.high) {
            t.high = t.last;
        }
        if (t.last < t.low) {
            t.low = t.last;
        }
        t.lastSize = packet.size;
        t.volume  += packet.size;
    }
    t.change    = PriceUtil::round(t.last - t.prevClose, security.decimals + 2);
    t.changePct = t.prevClose != 0 ? t.change / t.prevClose * 100.0 : 0;
    t.timestamp = packet.exchangeTimeMs;
    t.sequence  = ++d_sequence;
    *result = t;
    return true;
}

bool TickCache::get(Tick *result, const std::string& ticker) const
{
    LockGuard guard(&d_mutex);
    TickMap::const_iterator it = d_ticks.find(ticker);
    if (it == d_ticks.end()) {
        return false;
    }
    *result = it->second;
    return true;
}

void TickCache::snapshot(std::vector<Tick> *result) const
{
    LockGuard guard(&d_mutex);
    result->reserve(result->size() + d_ticks.size());
    for (TickMap::const_iterator it = d_ticks.begin();
         it != d_ticks.end();
         ++it) {
        result->push_back(it->second);
    }
}

Uint64 TickCache::lastSequence() const
{
    LockGuard guard(&d_mutex);
    return d_sequence;
}

int TickCache::size() const
{
    LockGuard guard(&d_mutex);
    return (int)d_ticks.size();
}

}  // close namespace mtsrv
