# Single source of truth for the CEF (Chromium Embedded Framework) version.
# scripts/get-cef.ps1 parses CEF_VERSION from this file, so keep the
# CEF_VERSION assignment below on a single line when bumping. See docs/UPGRADING.md.
set(CEF_VERSION "154.0.25+g1bc6331+chromium-154.0.8037.44")
set(CEF_PLATFORM "windows64")

set(CEF_DIST_NAME "cef_binary_${CEF_VERSION}_${CEF_PLATFORM}")
if(NOT DEFINED CEF_ROOT)
  set(CEF_ROOT "${CMAKE_SOURCE_DIR}/third_party/cef/${CEF_DIST_NAME}")
endif()
if(NOT EXISTS "${CEF_ROOT}/cmake/FindCEF.cmake")
  message(FATAL_ERROR
    "CEF ${CEF_VERSION} not found at ${CEF_ROOT}.\n"
    "Run scripts/get-cef.ps1 (or scripts/build.ps1) to download it.")
endif()
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
