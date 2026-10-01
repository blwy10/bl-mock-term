#pragma once

#include <cstdint>
#include <vector>

#include "types.h"

namespace mockterm::md {

// Generates deterministic (per ticker) mock OHLCV history that ends at
// |last_price| at |now_ms|. 1D = 1-minute bars for the trailing 6.5h,
// 1M = hourly bars for ~22 sessions, 1Y = daily bars for ~252 sessions.
std::vector<Bar> GenerateHistory(const Instrument& inst, HistoryRange range,
                                 double last_price, int64_t now_ms);

}  // namespace mockterm::md
