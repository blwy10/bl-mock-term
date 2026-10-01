// mtsrv_tickerplant.cpp                                              -*-C++-*-
#include "mtsrv_tickerplant.h"

#include <mtcore_log.h>
#include <mtcore_random.h>
#include <mtcore_time.h>

#include <algorithm>
#include <functional>

namespace mtsrv {

using mtcore::LockGuard;
using mtcore::TimeUtil;

namespace {

struct DeleteFeed : std::unary_function<FeedHandler *, void> {
    void operator()(FeedHandler *feed) const
    {
        delete feed;
    }
};

struct TickBySequence
: std::binary_function<const Tick&, const Tick&, bool> {
    bool operator()(const Tick& lhs, const Tick& rhs) const
    {
        return lhs.sequence < rhs.sequence;
    }
};

}  // close unnamed namespace

TickListener::~TickListener()
{
}

TickerPlantConfig::TickerPlantConfig()
: seed(0)
, intervalMs(25)
, tickProbability(0.2)
, dropProbability(0)
, barsPerSeries(1440)
{
}

TickerPlant::TickerPlant(const SecurityMaster    *master,
                         const TickerPlantConfig& config)
: d_master_p(master)
, d_config(config)
, d_store(config.barsPerSeries)
, d_runnable(this, &TickerPlant::run)
, d_thread("tickerplant")
, d_stopEvent(mtcore::Event::e_MANUAL_RESET)
, d_running(false)
, d_packets(0)
, d_trades(0)
, d_quotes(0)
, d_cycles(0)
{
    if (d_config.seed == 0) {
        d_config.seed = mtcore::Random::timeSeed();
    }
    mtcore::Random rng(d_config.seed);
    Int64 now = TimeUtil::nowMillis();

    std::vector<std::string> venues;
    master->exchanges(&venues);
    for (size_t v = 0; v < venues.size(); ++v) {
        FeedHandler *feed = new FeedHandler(venues[v],
                                            d_config.seed * 31 + v + 1);
        feed->setDropProbability(d_config.dropProbability);
        d_feeds.push_back(feed);

        std::vector<const Security *> members;
        master->securitiesOn(&members, venues[v]);
        for (size_t i = 0; i < members.size(); ++i) {
            const Security& sec = *members[i];
            double drift = rng.normal() * 0.006;
            double open  = sec.assetClass == AssetClass::e_RATE
                         ? sec.basePrice + drift
                         : sec.basePrice * (1.0 + drift);
            open = PriceUtil::round(open, sec.decimals);
            Int64 volume = sec.assetClass == AssetClass::e_EQUITY
                         ? (Int64)rng.range(200, 2000) * 1000
                         : 0;
            d_cache.initialize(sec, open, volume, now);
            feed->addSecurity(&sec, open);
        }
    }
}

TickerPlant::~TickerPlant()
{
    stop();
    std::for_each(d_feeds.begin(), d_feeds.end(), DeleteFeed());
}

void TickerPlant::addListener(TickListener *listener)
{
    LockGuard guard(&d_listenerMutex);
    d_listeners.push_back(listener);
}

void TickerPlant::removeListener(TickListener *listener)
{
    LockGuard guard(&d_listenerMutex);
    d_listeners.erase(std::remove(d_listeners.begin(),
                                  d_listeners.end(),
                                  listener),
                      d_listeners.end());
}

int TickerPlant::start()
{
    if (d_running) {
        return 0;
    }
    d_stopEvent.reset();
    d_running = true;
    if (d_thread.start(&d_runnable) != 0) {
        d_running = false;
        return -1;
    }
    MT_LOG_INFO("ticker plant started: %d feeds, %d instruments, seed=%I64u",
                (int)d_feeds.size(),
                d_cache.size(),
                d_config.seed);
    return 0;
}

void TickerPlant::stop()
{
    if (!d_running) {
        return;
    }
    d_running = false;
    d_stopEvent.set();
    d_thread.join();
    MT_LOG_INFO("ticker plant stopped after %I64u packets", d_packets);
}

void TickerPlant::run()
{
    while (d_running) {
        step(TimeUtil::nowMillis());
        if (d_stopEvent.wait(d_config.intervalMs)) {
            break;
        }
    }
}

void TickerPlant::step(Int64 nowMs)
{
    std::vector<FeedPacket> packets;
    for (FeedList::iterator it = d_feeds.begin(); it != d_feeds.end(); ++it) {
        (*it)->poll(&packets, nowMs, d_config.tickProbability);
    }

    std::vector<Tick> ticks;
    ticks.reserve(packets.size());
    for (size_t i = 0; i < packets.size(); ++i) {
        const FeedPacket& packet = packets[i];
        SequenceCheck::Value check = d_gaps.check(packet.feed,
                                                  packet.feedSequence);
        if (check == SequenceCheck::e_DUPLICATE) {
            continue;
        }
        if (check == SequenceCheck::e_GAP) {
            MT_LOG_DEBUG("feed %s: gap before sequence %I64u",
                         packet.feed.c_str(),
                         packet.feedSequence);
        }
        const Security *sec = d_master_p->find(packet.ticker);
        if (sec == NULL) {
            continue;
        }
        Tick tick;
        if (!d_cache.apply(&tick, packet, *sec)) {
            continue;
        }
        ++d_packets;
        if (packet.kind == FeedPacket::e_TRADE) {
            ++d_trades;
            d_store.record(tick);
        }
        else {
            ++d_quotes;
        }
        ticks.push_back(tick);
    }
    ++d_cycles;
    if (ticks.empty()) {
        return;
    }
    std::sort(ticks.begin(), ticks.end(), TickBySequence());

    ListenerList listeners;
    {
        LockGuard guard(&d_listenerMutex);
        listeners = d_listeners;
    }
    for (ListenerList::iterator l = listeners.begin();
         l != listeners.end();
         ++l) {
        for (std::vector<Tick>::const_iterator t = ticks.begin();
             t != ticks.end();
             ++t) {
            (*l)->onTick(*t);
        }
    }
}

const TickCache& TickerPlant::cache() const
{
    return d_cache;
}

const TimeSeriesStore& TickerPlant::store() const
{
    return d_store;
}

const GapDetector& TickerPlant::gaps() const
{
    return d_gaps;
}

const SecurityMaster& TickerPlant::master() const
{
    return *d_master_p;
}

int TickerPlant::numFeeds() const
{
    return (int)d_feeds.size();
}

const FeedHandler& TickerPlant::feed(int index) const
{
    return *d_feeds[index];
}

Uint64 TickerPlant::packetsProcessed() const
{
    return d_packets;
}

Uint64 TickerPlant::tradesProcessed() const
{
    return d_trades;
}

Uint64 TickerPlant::quotesProcessed() const
{
    return d_quotes;
}

Uint64 TickerPlant::cycles() const
{
    return d_cycles;
}

bool TickerPlant::isRunning() const
{
    return d_running;
}

}  // close namespace mtsrv
