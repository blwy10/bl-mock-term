# mtclient: reconnecting client session for the MockTerm wire protocol.
add_library(mtclient STATIC
  mtclient_session.cpp mtclient_session.h
)
target_include_directories(mtclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mtclient PUBLIC mtcore)
mockterm_legacy_target(mtclient)
