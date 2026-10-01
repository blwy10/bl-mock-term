#pragma once

#include <windows.h>

namespace mockterm::platform {

// Runs the CEF process (browser or sub-process) and returns its exit code.
int RunMain(HINSTANCE instance);

}  // namespace mockterm::platform
