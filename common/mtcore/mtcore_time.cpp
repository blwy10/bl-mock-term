// mtcore_time.cpp                                                    -*-C++-*-
#include "mtcore_time.h"

#include <stdio.h>

namespace mtcore {

const Int64 TimeUtil::k_MILLIS_PER_DAY = 86400000;

namespace {

// 100ns intervals between 1601-01-01 and 1970-01-01.
const Int64 k_EPOCH_DELTA = 116444736000000000LL;

}  // close unnamed namespace

Int64 TimeUtil::nowMillis()
{
    FILETIME ft;
    ::GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER li;
    li.LowPart  = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    return ((Int64)li.QuadPart - k_EPOCH_DELTA) / 10000;
}

Int64 TimeUtil::monotonicMicros()
{
    static LARGE_INTEGER s_frequency = { 0 };
    if (s_frequency.QuadPart == 0) {
        ::QueryPerformanceFrequency(&s_frequency);
    }
    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);
    return (Int64)(now.QuadPart * 1000000 / s_frequency.QuadPart);
}

std::string TimeUtil::formatIso(Int64 epochMillis)
{
    ULARGE_INTEGER li;
    li.QuadPart = (ULONGLONG)(epochMillis * 10000 + k_EPOCH_DELTA);
    FILETIME ft;
    ft.dwLowDateTime  = li.LowPart;
    ft.dwHighDateTime = li.HighPart;
    SYSTEMTIME st;
    ::FileTimeToSystemTime(&ft, &st);
    char buf[64];
    _snprintf(buf,
              sizeof(buf),
              "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
              st.wYear,
              st.wMonth,
              st.wDay,
              st.wHour,
              st.wMinute,
              st.wSecond,
              st.wMilliseconds);
    return std::string(buf);
}

std::string TimeUtil::formatTime(Int64 epochMillis)
{
    Int64 secondsOfDay = (epochMillis / 1000) % 86400;
    char buf[16];
    _snprintf(buf,
              sizeof(buf),
              "%02d:%02d:%02d",
              (int)(secondsOfDay / 3600),
              (int)(secondsOfDay / 60 % 60),
              (int)(secondsOfDay % 60));
    return std::string(buf);
}

Int64 TimeUtil::floorToMinute(Int64 epochMillis)
{
    return epochMillis - epochMillis % k_MILLIS_PER_MINUTE;
}

}  // close namespace mtcore
