#pragma once

#include <functional>

namespace mockterm::platform {

// Runs |fn| on the CEF browser-process UI thread.
void PostUiTask(std::function<void()> fn);

}  // namespace mockterm::platform
