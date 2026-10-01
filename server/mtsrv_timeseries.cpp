// mtsrv_timeseries.cpp                                               -*-C++-*-
#include "mtsrv_timeseries.h"

#include <mtcore_time.h>

namespace mtsrv {

using mtcore::LockGuard;
using mtcore::TimeUtil;

                                // -------------
                                // class BarRing
                                // -------------

BarRing::BarRing(int capacity)
: d_bars_p(new Bar[capacity])
, d_capacity(capacity)
, d_head(-1)
, d_count(0)
{
}

BarRing::~BarRing()
{
    delete [] d_bars_p;
}

void BarRing::onTrade(Int64 timestampMs, double price, Int64 size)
{
    Int64 minute = TimeUtil::floorToMinute(timestampMs);
    if (d_count > 0 && d_bars_p[d_head].timestamp == minute) {
        Bar& bar = d_bars_p[d_head];
        if (price > bar.high) {
            bar.high = price;
        }
        if (price < bar.low) {
            bar.low = price;
        }
        bar.close   = price;
        bar.volume += size;
        return;
    }
    if (d_count > 0 && minute < d_bars_p[d_head].timestamp) {
        return;  // out-of-order trade for a closed minute
    }
    d_head = (d_head + 1) % d_capacity;
    if (d_count < d_capacity) {
        ++d_count;
    }
    Bar& bar     = d_bars_p[d_head];
    bar.timestamp = minute;
    bar.open      = price;
    bar.high      = price;
    bar.low       = price;
    bar.close     = price;
    bar.volume    = size;
}

int BarRing::copy(std::vector<Bar> *result, Int64 fromMs) const
{
    int appended = 0;
    int start    = (d_head - d_count + 1 + d_capacity) % d_capacity;
    for (int i = 0; i < d_count; ++i) {
        const Bar& bar = d_bars_p[(start + i) % d_capacity];
        if (bar.timestamp >= fromMs) {
            result->push_back(bar);
            ++appended;
        }
    }
    return appended;
}

int BarRing::count() const
{
    return d_count;
}

int BarRing::capacity() const
{
    return d_capacity;
}

const Bar *BarRing::newest() const
{
    return d_count > 0 ? &d_bars_p[d_head] : NULL;
}

                            // ---------------------
                            // class TimeSeriesStore
                            // ---------------------

TimeSeriesStore::TimeSeriesStore(int barsPerSeries)
: d_capacity(barsPerSeries)
, d_recorded(0)
{
}

TimeSeriesStore::~TimeSeriesStore()
{
    for (SeriesMap::iterator it = d_series.begin();
         it != d_series.end();
         ++it) {
        delete it->second;
    }
}

void TimeSeriesStore::record(const Tick& tick)
{
    if (tick.lastSize <= 0) {
        return;
    }
    LockGuard guard(&d_mutex);
    SeriesMap::iterator it = d_series.find(tick.ticker);
    BarRing *ring;
    if (it == d_series.end()) {
        ring = new BarRing(d_capacity);
        d_series[tick.ticker] = ring;
    }
    else {
        ring = it->second;
    }
    ring->onTrade(tick.timestamp, tick.last, tick.lastSize);
    ++d_recorded;
}

int TimeSeriesStore::intradayBars(std::vector<Bar>  *result,
                                  const std::string& ticker,
                                  Int64              fromMs) const
{
    LockGuard guard(&d_mutex);
    SeriesMap::const_iterator it = d_series.find(ticker);
    if (it == d_series.end()) {
        return 0;
    }
    return it->second->copy(result, fromMs);
}

Uint64 TimeSeriesStore::ticksRecorded() const
{
    return d_recorded;
}

int TimeSeriesStore::seriesCount() const
{
    LockGuard guard(&d_mutex);
    return (int)d_series.size();
}

}  // close namespace mtsrv
