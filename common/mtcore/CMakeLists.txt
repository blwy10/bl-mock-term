# mtcore: shared foundation for mtserver, mtcomm, mtcli and the mockterm
# session client. Pre-C++11 style; compiled as C++14.
add_library(mtcore STATIC
  mtcore_codec.cpp mtcore_codec.h
  mtcore_connection.cpp mtcore_connection.h
  mtcore_element.cpp mtcore_element.h
  mtcore_log.cpp mtcore_log.h
  mtcore_message.cpp mtcore_message.h
  mtcore_mutex.cpp mtcore_mutex.h
  mtcore_platform.h
  mtcore_process.cpp mtcore_process.h
  mtcore_random.cpp mtcore_random.h
  mtcore_refcount.h
  mtcore_socket.cpp mtcore_socket.h
  mtcore_stringutil.cpp mtcore_stringutil.h
  mtcore_thread.cpp mtcore_thread.h
  mtcore_threadpool.cpp mtcore_threadpool.h
  mtcore_time.cpp mtcore_time.h
)
target_include_directories(mtcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mtcore PUBLIC ws2_32)
mockterm_legacy_target(mtcore)
