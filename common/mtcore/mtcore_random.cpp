// mtcore_random.cpp                                                  -*-C++-*-
#include "mtcore_random.h"

#include <math.h>

namespace mtcore {

Random::Random(Uint64 seedValue)
{
    seed(seedValue);
}

void Random::seed(Uint64 seedValue)
{
    Uint64 z = seedValue + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z = z ^ (z >> 31);
    d_state    = z ? z : 0x2545F4914F6CDD1DULL;
    d_spare    = 0;
    d_hasSpare = false;
}

Uint64 Random::next()
{
    d_state ^= d_state >> 12;
    d_state ^= d_state << 25;
    d_state ^= d_state >> 27;
    return d_state * 0x2545F4914F6CDD1DULL;
}

double Random::uniform()
{
    return (double)(next() >> 11) * (1.0 / 9007199254740992.0);
}

double Random::normal()
{
    if (d_hasSpare) {
        d_hasSpare = false;
        return d_spare;
    }
    double u, v, s;
    do {
        u = uniform() * 2.0 - 1.0;
        v = uniform() * 2.0 - 1.0;
        s = u * u + v * v;
    } while (s >= 1.0 || s == 0.0);
    double m = sqrt(-2.0 * log(s) / s);
    d_spare    = v * m;
    d_hasSpare = true;
    return u * m;
}

int Random::range(int low, int high)
{
    if (high <= low) {
        return low;
    }
    Uint64 span = (Uint64)(high - low) + 1;
    return low + (int)(next() % span);
}

bool Random::chance(double probability)
{
    return uniform() < probability;
}

Uint64 Random::timeSeed()
{
    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);
    return (Uint64)now.QuadPart ^ ((Uint64)::GetCurrentProcessId() << 32);
}

}  // close namespace mtcore
