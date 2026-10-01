# Compile settings for the pre-C++11-style back end (mtcore, mtserver, mtcomm,
# mtcli). These targets are deliberately held at C++14 so that idioms removed
# in C++17/20 (std::auto_ptr, std::unary_function, std::bind1st, dynamic
# exception specifications) still compile. See docs/MODERNISATION.md.
function(mockterm_legacy_target target)
  set_target_properties(${target} PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
  target_compile_definitions(${target} PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    _WINSOCK_DEPRECATED_NO_WARNINGS _HAS_AUTO_PTR_ETC=1)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W3 /wd4996)
  endif()
endfunction()

# Executables land next to mockterm.exe so the host can auto-spawn them.
function(mockterm_runtime_dir target)
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/host/$<CONFIG>")
endfunction()
