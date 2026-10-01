// mtsrv_history.cpp                                                  -*-C++-*-
#include "mtsrv_history.h"

#include <mtcore_random.h>
#include <mtcore_stringutil.h>

#include <math.h>

namespace mtsrv {

namespace {

const Int64 k_DAY_MS = 86400000;

double maxOf(double a, double b)
{
    return a > b ? a : b;
}

double minOf(double a, double b)
{
    return a < b ? a : b;
}

}  // close unnamed namespace

int HistoryGenerator::barCount(HistoryRange::Value range)
{
    switch (range) {
      case HistoryRange::e_ONE_DAY:   return 390;
      case HistoryRange::e_ONE_MONTH: return 22 * 7;
      case HistoryRange::e_ONE_YEAR:  return 252;
    }
    return 0;
}

void HistoryGenerator::generate(std::vector<Bar>   *result,
                                const Security&     security,
                                HistoryRange::Value range,
                                double              lastPrice,
                                Int64               nowMs)
{
    int    count  = barCount(range);
    Int64  stepMs = 0;
    double vol    = 0;
    switch (range) {
      case HistoryRange::e_ONE_DAY:
        stepMs = 60000;
        vol    = 0.0009;
        break;
      case HistoryRange::e_ONE_MONTH:
        stepMs = 30 * k_DAY_MS / (count - 1);
        vol    = 0.004;
        break;
      case HistoryRange::e_ONE_YEAR:
        stepMs = 365 * k_DAY_MS / (count - 1);
        vol    = 0.013;
        break;
    }
    if (security.assetClass == AssetClass::e_CURRENCY) {
        vol *= 0.35;
    }
    if (security.assetClass == AssetClass::e_INDEX) {
        vol *= 0.7;
    }
    bool additive = security.assetClass == AssetClass::e_RATE;
    if (additive) {
        vol = range == HistoryRange::e_ONE_DAY   ? 0.004
            : range == HistoryRange::e_ONE_MONTH ? 0.012
            :                                      0.04;
    }

    mtcore::Uint64 seed = mtcore::StringUtil::hash(security.ticker);
    seed = (seed << 8) ^ (mtcore::Uint64)(range + 1);
    mtcore::Random rng(seed);

    // Walk backwards from the current price so the series ends there.
    double *closes = new double[count];
    double p = lastPrice;
    for (int i = count - 1; i >= 0; --i) {
        closes[i] = p;
        if (additive) {
            p = maxOf(0.001, p - rng.normal() * vol);
        }
        else {
            p = p / exp(rng.normal() * vol);
        }
    }

    int dp = security.decimals;
    result->clear();
    result->resize(count);
    double prev  = p;
    Int64  start = nowMs - (Int64)(count - 1) * stepMs;
    for (int i = 0; i < count; ++i) {
        Bar& b = (*result)[i];
        b.timestamp = start + (Int64)i * stepMs;
        b.open      = PriceUtil::round(prev, dp);
        b.close     = PriceUtil::round(closes[i], dp);
        double wiggle = additive ? vol * 0.5 : fabs(closes[i]) * vol * 0.5;
        b.high = PriceUtil::round(maxOf(b.open, b.close)
                                      + rng.uniform() * wiggle,
                                  dp);
        b.low  = PriceUtil::round(minOf(b.open, b.close)
                                      - rng.uniform() * wiggle,
                                  dp);
        if (security.assetClass == AssetClass::e_EQUITY) {
            b.volume = (Int64)(50000 + rng.uniform() * 450000)
                     * (range == HistoryRange::e_ONE_YEAR ? 60 : 1);
        }
        else {
            b.volume = 0;
        }
        prev = closes[i];
    }
    delete [] closes;
}

int HistoryGenerator::overlay(std::vector<Bar>       *bars,
                              const std::vector<Bar>& live)
{
    if (bars->empty() || live.empty()) {
        return 0;
    }
    size_t n = live.size() < bars->size() ? live.size() : bars->size();
    size_t offset = bars->size() - n;
    size_t liveOffset = live.size() - n;
    for (size_t i = 0; i < n; ++i) {
        (*bars)[offset + i] = live[liveOffset + i];
    }
    if (offset > 0) {
        // Stitch the synthetic series onto the first captured bar.
        Bar& seam = (*bars)[offset];
        Bar& before = (*bars)[offset - 1];
        before.close = seam.open;
        before.high  = maxOf(before.high, before.close);
        before.low   = minOf(before.low, before.close);
    }
    return (int)n;
}

}  // close namespace mtsrv
