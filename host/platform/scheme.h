#pragma once

#include <filesystem>

#include "include/cef_scheme.h"

namespace mockterm::platform {

// Registers mockterm:// as a standard, secure scheme. Must run in every
// process (called from CefApp::OnRegisterCustomSchemes).
void RegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar);

// Serves files under |apps_dir| for mockterm://apps/<path>. Browser process.
void RegisterAppSchemeHandler(const std::filesystem::path& apps_dir);

}  // namespace mockterm::platform
