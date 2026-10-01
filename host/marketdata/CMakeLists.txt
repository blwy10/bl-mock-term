# Pure C++ market-data simulation. Must not depend on CEF.
add_library(marketdata STATIC
  chat.cpp chat.h
  history.cpp history.h
  instruments.cpp instruments.h
  json.cpp json.h
  news.cpp news.h
  service.cpp service.h
  simulator.cpp simulator.h
  subscription_manager.cpp subscription_manager.h
  types.h
)
target_include_directories(marketdata PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(marketdata PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(marketdata PRIVATE /W4 /permissive-)
endif()
