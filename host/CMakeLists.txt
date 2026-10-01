# MockTerm host executable. Direct CEF usage is confined to platform/.
set(MOCKTERM_HOST_SRCS
  main.cpp
  platform/app.cpp platform/app.h
  platform/browser_bridge.cpp platform/browser_bridge.h
  platform/client.cpp platform/client.h
  platform/entry.cpp platform/entry.h
  platform/ipc.h
  platform/renderer_bridge.cpp platform/renderer_bridge.h
  platform/scheme.cpp platform/scheme.h
  platform/ui_task.cpp platform/ui_task.h
)

ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
SET_CEF_TARGET_OUT_DIR()

add_executable(mockterm WIN32 ${MOCKTERM_HOST_SRCS})
SET_EXECUTABLE_TARGET_PROPERTIES(mockterm)
add_dependencies(mockterm libcef_dll_wrapper)
target_include_directories(mockterm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mockterm libcef_lib libcef_dll_wrapper marketdata ${CEF_STANDARD_LIBS})

ADD_WINDOWS_MANIFEST("${CMAKE_CURRENT_SOURCE_DIR}/win" "mockterm" "exe")

# CEF runtime next to the executable.
COPY_FILES(mockterm "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
COPY_FILES(mockterm "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")

# Bundled TypeScript apps, served via mockterm://apps/.
set(MOCKTERM_APPS_DIST "${CMAKE_SOURCE_DIR}/apps/dist")
add_custom_target(mockterm_apps ALL
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${CEF_TARGET_OUT_DIR}/apps"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${MOCKTERM_APPS_DIST}" "${CEF_TARGET_OUT_DIR}/apps"
  COMMENT "Copying apps/dist -> apps/"
)
add_dependencies(mockterm mockterm_apps)
