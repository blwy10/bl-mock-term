// mtsrv_timeseries.h                                                 -*-C++-*-
#ifndef INCLUDED_MTSRV_TIMESERIES
#define INCLUDED_MTSRV_TIMESERIES

//@PURPOSE: Store intraday one-minute bars built from live ticks.
//
//@CLASSES:
//  mtsrv::BarRing:          fixed-capacity circular buffer of bars
//  mtsrv::TimeSeriesStore:  ticker -> 'BarRing', thread-safe

#include "mtsrv_tick.h"

#include <mtcore_mutex.h>

#include <map>
#include <string>
#include <vector>

namespace mtsrv {

class BarRing {
    Bar *d_bars_p;     // owned array of 'd_capacity'
    int  d_capacity;
    int  d_head;       // index of the newest bar
    int  d_count;

  private:
    // NOT IMPLEMENTED
    BarRing(const BarRing&);
    BarRing& operator=(const BarRing&);

  public:
    explicit BarRing(int capacity);
    ~BarRing();

    void onTrade(Int64 timestampMs, double price, Int64 size);
        // Update the bar for the minute containing 'timestampMs', opening a
        // new bar (and evicting the oldest) when the minute rolls.

    int copy(std::vector<Bar> *result, Int64 fromMs) const;
        // Append bars starting at or after 'fromMs', oldest first.  Return
        // the number appended.

    int count() const;
    int capacity() const;
    const Bar *newest() const;
};

class TimeSeriesStore {
    typedef std::map<std::string, BarRing *> SeriesMap;

    mutable mtcore::Mutex d_mutex;
    SeriesMap             d_series;     // owns the rings
    int                   d_capacity;
    volatile Uint64       d_recorded;

  private:
    // NOT IMPLEMENTED
    TimeSeriesStore(const TimeSeriesStore&);
    TimeSeriesStore& operator=(const TimeSeriesStore&);

  public:
    explicit TimeSeriesStore(int barsPerSeries = 1440);
    ~TimeSeriesStore();

    void record(const Tick& tick);
        // Only ticks with 'lastSize > 0' (trades) update bars.

    int intradayBars(std::vector<Bar>  *result,
                     const std::string& ticker,
                     Int64              fromMs) const;

    Uint64 ticksRecorded() const;
    int seriesCount() const;
};

}  // close namespace mtsrv

#endif
