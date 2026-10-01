// mtsrv_tickerplant.h                                                -*-C++-*-
#ifndef INCLUDED_MTSRV_TICKERPLANT
#define INCLUDED_MTSRV_TICKERPLANT

//@PURPOSE: Run the simulated ticker plant: feeds -> sequencing -> cache.
//
//@CLASSES:
//  mtsrv::TickListener:       callback for every normalized tick
//  mtsrv::TickerPlantConfig:  tuning parameters
//  mtsrv::TickerPlant:        owns feed handlers, gap detector, cache, store
//
//@DESCRIPTION: A dedicated thread wakes every 'intervalMs', polls each feed
// handler for raw packets, checks per-feed sequence numbers, normalizes the
// packets into the last-value cache, records trades in the time-series store
// and fans the resulting ticks out to listeners in plant-sequence order.
//..
//  FeedHandler(XNAS) --+
//  FeedHandler(XNYS) --+--> GapDetector --> TickCache --> TimeSeriesStore
//  FeedHandler(...)  --+                        |
//                                               +--> TickListener(s)
//..

#include "mtsrv_feedhandler.h"
#include "mtsrv_gapdetector.h"
#include "mtsrv_security.h"
#include "mtsrv_tickcache.h"
#include "mtsrv_timeseries.h"

#include <mtcore_mutex.h>
#include <mtcore_thread.h>

#include <vector>

namespace mtsrv {

class TickListener {
  public:
    virtual ~TickListener();
    virtual void onTick(const Tick& tick) = 0;
};

struct TickerPlantConfig {
    Uint64 seed;              // 0 => time-derived
    int    intervalMs;
    double tickProbability;   // per instrument per interval
    double dropProbability;   // simulated packet loss per feed packet
    int    barsPerSeries;

    TickerPlantConfig();
};

class TickerPlant {
    typedef std::vector<FeedHandler *>  FeedList;
    typedef std::vector<TickListener *> ListenerList;

    const SecurityMaster            *d_master_p;
    TickerPlantConfig                d_config;
    FeedList                         d_feeds;          // owned
    GapDetector                      d_gaps;
    TickCache                        d_cache;
    TimeSeriesStore                  d_store;
    mtcore::Mutex                    d_listenerMutex;
    ListenerList                     d_listeners;
    mtcore::MemberRunnable<TickerPlant> d_runnable;
    mtcore::Thread                   d_thread;
    mtcore::Event                    d_stopEvent;
    volatile bool                    d_running;
    volatile Uint64                  d_packets;
    volatile Uint64                  d_trades;
    volatile Uint64                  d_quotes;
    volatile Uint64                  d_cycles;

  private:
    void run();

    // NOT IMPLEMENTED
    TickerPlant(const TickerPlant&);
    TickerPlant& operator=(const TickerPlant&);

  public:
    TickerPlant(const SecurityMaster *master, const TickerPlantConfig& config);
        // Build one feed handler per exchange in 'master' and seed the cache.
    ~TickerPlant();

    void addListener(TickListener *listener);
    void removeListener(TickListener *listener);

    int start();
    void stop();

    void step(Int64 nowMs);
        // Run one polling cycle synchronously (used by 'run' and by tests).

    const TickCache& cache() const;
    const TimeSeriesStore& store() const;
    const GapDetector& gaps() const;
    const SecurityMaster& master() const;
    int numFeeds() const;
    const FeedHandler& feed(int index) const;
    Uint64 packetsProcessed() const;
    Uint64 tradesProcessed() const;
    Uint64 quotesProcessed() const;
    Uint64 cycles() const;
    bool isRunning() const;
};

}  // close namespace mtsrv

#endif
