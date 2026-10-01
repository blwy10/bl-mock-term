# mtserver: simulated remote market-data server (see docs/ARCHITECTURE.md).
add_library(mtserverlib STATIC
  mtsrv_chatengine.cpp mtsrv_chatengine.h
  mtsrv_clientsession.cpp mtsrv_clientsession.h
  mtsrv_config.cpp mtsrv_config.h
  mtsrv_feedhandler.cpp mtsrv_feedhandler.h
  mtsrv_fields.cpp mtsrv_fields.h
  mtsrv_gapdetector.cpp mtsrv_gapdetector.h
  mtsrv_history.cpp mtsrv_history.h
  mtsrv_mktdataservice.cpp mtsrv_mktdataservice.h
  mtsrv_msgservice.cpp mtsrv_msgservice.h
  mtsrv_newsengine.cpp mtsrv_newsengine.h
  mtsrv_newsservice.cpp mtsrv_newsservice.h
  mtsrv_refdataservice.cpp mtsrv_refdataservice.h
  mtsrv_security.cpp mtsrv_security.h
  mtsrv_server.cpp mtsrv_server.h
  mtsrv_service.cpp mtsrv_service.h
  mtsrv_subscriptiontable.cpp mtsrv_subscriptiontable.h
  mtsrv_tick.cpp mtsrv_tick.h
  mtsrv_tickcache.cpp mtsrv_tickcache.h
  mtsrv_tickerplant.cpp mtsrv_tickerplant.h
  mtsrv_timeseries.cpp mtsrv_timeseries.h
)
target_include_directories(mtserverlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mtserverlib PUBLIC mtcore)
mockterm_legacy_target(mtserverlib)

add_executable(mtserver main.cpp)
target_link_libraries(mtserver PRIVATE mtserverlib)
mockterm_legacy_target(mtserver)
mockterm_runtime_dir(mtserver)
