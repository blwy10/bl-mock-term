cmake_minimum_required(VERSION 3.21)

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING "" FORCE)
project(mockterm LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# CEF's wrapper is built with the static CRT (/MT); match it everywhere.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)

option(MOCKTERM_BUILD_HOST "Build the CEF host executable (requires CEF)" ON)
option(MOCKTERM_BUILD_TESTS "Build the C++ unit tests" ON)

enable_testing()

include(cmake/legacy.cmake)
add_subdirectory(common/mtcore)
add_subdirectory(server)
add_subdirectory(host/marketdata)

if(MOCKTERM_BUILD_TESTS)
  add_subdirectory(host/tests)
endif()

if(MOCKTERM_BUILD_HOST)
  set(USE_SANDBOX OFF CACHE BOOL "MockTerm runs CEF without the sandbox" FORCE)
  include(cmake/cef.cmake)
  find_package(CEF REQUIRED)
  add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)
  add_subdirectory(host)
  PRINT_CEF_CONFIG()
endif()
