# mtcomm: local communications process (bbcomm analogue) and mtcli tool.
add_library(mtcommlib STATIC
  mtcomm_config.cpp mtcomm_config.h
  mtcomm_conflator.cpp mtcomm_conflator.h
  mtcomm_engine.cpp mtcomm_engine.h
  mtcomm_localclient.cpp mtcomm_localclient.h
  mtcomm_lvc.cpp mtcomm_lvc.h
  mtcomm_topicmux.cpp mtcomm_topicmux.h
)
target_include_directories(mtcommlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mtcommlib PUBLIC mtclient mtcore)
mockterm_legacy_target(mtcommlib)

add_executable(mtcomm main.cpp)
target_link_libraries(mtcomm PRIVATE mtcommlib)
mockterm_legacy_target(mtcomm)
mockterm_runtime_dir(mtcomm)

add_executable(mtcli mtcli.cpp)
target_link_libraries(mtcli PRIVATE mtclient mtcore)
mockterm_legacy_target(mtcli)
mockterm_runtime_dir(mtcli)
