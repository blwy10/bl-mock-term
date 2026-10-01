# Terminal-side data layer: an mtcomm client plus the renderer-facing types
# and JSON. Must not depend on CEF.
add_library(marketdata STATIC
  comms_client.cpp comms_client.h
  convert.cpp convert.h
  json.cpp json.h
  types.cpp types.h
)
target_include_directories(marketdata PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(marketdata PUBLIC cxx_std_20)
target_link_libraries(marketdata PUBLIC mtclient mtcore)
target_compile_definitions(marketdata PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
  target_compile_options(marketdata PRIVATE /W4 /permissive-)
endif()
