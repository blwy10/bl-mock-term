include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
  URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(marketdata_tests
  simulator_test.cpp
  subscription_manager_test.cpp
  service_test.cpp
)
target_link_libraries(marketdata_tests PRIVATE marketdata GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(marketdata_tests DISCOVERY_MODE PRE_TEST)
