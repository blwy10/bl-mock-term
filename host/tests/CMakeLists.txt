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
  comms_client_test.cpp
  convert_test.cpp
)
target_link_libraries(marketdata_tests PRIVATE marketdata GTest::gtest_main)
target_compile_definitions(marketdata_tests PRIVATE
  WIN32_LEAN_AND_MEAN NOMINMAX MTCOMM_EXE="$<TARGET_FILE:mtcomm>")
add_dependencies(marketdata_tests mtcomm mtserver)
include(GoogleTest)
gtest_discover_tests(marketdata_tests DISCOVERY_MODE PRE_TEST)

# Legacy back end (mtcore, mtserver, mtcomm). Compiled with the same C++14
# settings as the code under test.
add_executable(backend_tests
  backend_integration_test.cpp
  backend_unit_test.cpp
)
target_link_libraries(backend_tests PRIVATE mtcommlib mtserverlib GTest::gtest_main)
mockterm_legacy_target(backend_tests)
gtest_discover_tests(backend_tests DISCOVERY_MODE PRE_TEST)
