// mtsrv_tick.cpp                                                     -*-C++-*-
#include "mtsrv_tick.h"

#include <math.h>

namespace mtsrv {

Tick::Tick()
: bid(0)
, ask(0)
, last(0)
, open(0)
, high(0)
, low(0)
, prevClose(0)
, change(0)
, changePct(0)
, volume(0)
, lastSize(0)
, timestamp(0)
, sequence(0)
{
}

Bar::Bar()
: timestamp(0)
, open(0)
, high(0)
, low(0)
, close(0)
, volume(0)
{
}

const char *HistoryRange::toString(Value value)
{
    switch (value) {
      case e_ONE_DAY:   return "1D";
      case e_ONE_MONTH: return "1M";
      case e_ONE_YEAR:  return "1Y";
    }
    return "?";
}

int HistoryRange::fromString(Value *result, const std::string& text)
{
    if (text == "1D") { *result = e_ONE_DAY;   return 0; }
    if (text == "1M") { *result = e_ONE_MONTH; return 0; }
    if (text == "1Y") { *result = e_ONE_YEAR;  return 0; }
    return -1;
}

double PriceUtil::round(double value, int decimals)
{
    double f = pow(10.0, decimals);
    return floor(value * f + 0.5) / f;
}

double PriceUtil::tickSize(int decimals)
{
    return pow(10.0, -decimals);
}

}  // close namespace mtsrv
