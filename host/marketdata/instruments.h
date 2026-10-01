#pragma once

#include <vector>

#include "types.h"

namespace mockterm::md {

// The fixed universe of simulated instruments.
const std::vector<Instrument>& DefaultInstruments();

}  // namespace mockterm::md
