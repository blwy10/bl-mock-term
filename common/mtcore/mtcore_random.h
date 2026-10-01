// mtcore_random.h                                                    -*-C++-*-
#ifndef INCLUDED_MTCORE_RANDOM
#define INCLUDED_MTCORE_RANDOM

//@PURPOSE: Provide a small, seedable pseudo-random number generator.
//
//@DESCRIPTION: 'Random' is xorshift64* seeded through splitmix64, with a
// Box-Muller normal deviate.  Sequences are identical on every platform for
// a given seed, which the simulators and tests rely on.

#include "mtcore_platform.h"

namespace mtcore {

class Random {
    Uint64 d_state;
    double d_spare;
    bool   d_hasSpare;

  public:
    explicit Random(Uint64 seed = 0);

    void seed(Uint64 seed);

    Uint64 next();
    double uniform();
        // Uniform in [0, 1).
    double normal();
        // Standard normal deviate.
    int range(int low, int high);
        // Uniform integer in [low, high].
    bool chance(double probability);

    static Uint64 timeSeed();
};

}  // close namespace mtcore

#endif
