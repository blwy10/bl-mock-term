// mtsrv_feedhandler.cpp                                              -*-C++-*-
#include "mtsrv_feedhandler.h"

#include "mtsrv_tick.h"

#include <math.h>

namespace mtsrv {

FeedPacket::FeedPacket()
: feedSequence(0)
, kind(e_TRADE)
, bid(0)
, ask(0)
, price(0)
, size(0)
, exchangeTimeMs(0)
{
}

FeedHandler::FeedHandler(const std::string& feedId, Uint64 seed)
: d_feedId(feedId)
, d_rng(seed)
, d_nextSequence(1)
, d_dropProbability(0)
, d_dropped(0)
, d_emitted(0)
{
}

void FeedHandler::addSecurity(const Security *security, double openPrice)
{
    InstrumentState state;
    state.d_security_p = security;
    state.d_last       = openPrice;
    d_instruments.push_back(state);
}

void FeedHandler::setDropProbability(double probability)
{
    d_dropProbability = probability;
}

void FeedHandler::poll(std::vector<FeedPacket> *packets,
                       Int64                    nowMs,
                       double                   tickProbability)
{
    for (StateList::iterator it = d_instruments.begin();
         it != d_instruments.end();
         ++it) {
        if (!d_rng.chance(tickProbability)) {
            continue;
        }
        const Security& sec = *it->d_security_p;
        bool isTrade = d_rng.chance(0.75);

        if (isTrade) {
            double shock = d_rng.normal() * sec.volatility;
            double next;
            if (sec.assetClass == AssetClass::e_RATE) {
                next = it->d_last + shock;
            }
            else {
                // Mean-revert gently towards the reference close.
                double pull = (sec.basePrice - it->d_last) / sec.basePrice;
                next = it->d_last * (1.0 + shock + pull * 0.002);
            }
            double floorPrice = PriceUtil::tickSize(sec.decimals);
            if (next < floorPrice) {
                next = floorPrice;
            }
            it->d_last = PriceUtil::round(next, sec.decimals);
        }

        double halfSpread = it->d_last * sec.spreadBps / 20000.0;
        double jitter     = 1.0 + 0.5 * d_rng.uniform();
        double tickSize   = PriceUtil::tickSize(sec.decimals);
        if (halfSpread * jitter < tickSize) {
            halfSpread = tickSize;
            jitter     = 1.0;
        }

        Uint64 sequence = d_nextSequence++;
        if (d_dropProbability > 0 && d_rng.chance(d_dropProbability)) {
            ++d_dropped;
            continue;
        }

        FeedPacket packet;
        packet.feed           = d_feedId;
        packet.feedSequence   = sequence;
        packet.kind           = isTrade ? FeedPacket::e_TRADE
                                        : FeedPacket::e_QUOTE;
        packet.ticker         = sec.ticker;
        packet.price          = it->d_last;
        packet.bid            = PriceUtil::round(it->d_last
                                                     - halfSpread * jitter,
                                                 sec.decimals);
        packet.ask            = PriceUtil::round(it->d_last
                                                     + halfSpread * jitter,
                                                 sec.decimals);
        packet.size           = isTrade ? (Int64)d_rng.range(1, 50) * 100 : 0;
        packet.exchangeTimeMs = nowMs;
        packets->push_back(packet);
        ++d_emitted;
    }
}

const std::string& FeedHandler::feedId() const
{
    return d_feedId;
}

int FeedHandler::numInstruments() const
{
    return (int)d_instruments.size();
}

Uint64 FeedHandler::nextSequence() const
{
    return d_nextSequence;
}

Uint64 FeedHandler::droppedPackets() const
{
    return d_dropped;
}

Uint64 FeedHandler::emittedPackets() const
{
    return d_emitted;
}

}  // close namespace mtsrv
