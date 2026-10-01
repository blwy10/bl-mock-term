# Upgrading CEF

MockTerm uses the prebuilt **Standard Distribution** for `windows64` from the
[CEF Automated Builds](https://cef-builds.spotifycdn.com/index.html) site.

## Source of truth

The version is pinned in one line of `cmake/cef.cmake`:

```cmake
set(CEF_VERSION "154.0.25+g1bc6331+chromium-154.0.8037.44")
```

`scripts/get-cef.ps1` reads that value, looks up the matching archive and its
SHA1 in `https://cef-builds.spotifycdn.com/index.json`, downloads it, verifies
the hash and extracts it to `third_party/cef/cef_binary_<version>_windows64/`.
CMake locates it from the same value.

## Procedure

1. Pick a version from the builds index (prefer the current *Stable* channel)
   and copy the full version string, e.g. `155.0.3+gabcdef0+chromium-155.0.8100.12`.
2. Edit `CEF_VERSION` in `cmake/cef.cmake`. Update the version table in `README.md`.
3. Run `scripts\build.ps1 -Clean` to download the new distribution and rebuild.
4. Fix compile errors. They should all be in `host/platform/`, the only code that
   includes CEF headers. Diff the distribution's `include/` and read the CEF
   release notes; areas most likely to change:
   - `CefSettings` / `CefWindowInfo` fields (`entry.cpp`, `app.cpp`)
   - `CefFrame::GetIdentifier()` type and `CefFrameHandler` callbacks (`client.cpp`, `renderer_bridge.cpp`)
   - V8 promise API (`CefV8Value::CreatePromise`, `ResolvePromise`, `RejectPromise`)
   - `CefSchemeRegistrar` option flags and `CefStreamResourceHandler` (`scheme.cpp`)
   - Runtime style defaults (MockTerm uses the Alloy style in a native popup window)
5. Binary/resource file lists come from CEF's `cmake/cef_variables.cmake` via
   `CEF_BINARY_FILES` / `CEF_RESOURCE_FILES`, so `host/CMakeLists.txt` normally
   needs no change.
6. Run all tests: `ctest`, `cd apps; npm test`, `cd scripts; npm run e2e`.
7. Delete the old `third_party/cef/cef_binary_*` directory and archive.

## Notes

- `libcef_dll_wrapper` is built from the distribution's sources as part of the
  CMake build, so its CRT (`/MT`) always matches the host.
- `third_party/cef/` is git-ignored; machines fetch it with `get-cef.ps1`.
- If Windows `tar.exe` fails to extract the `.tar.bz2`, extract it manually
  (7-Zip, or Git Bash `tar -xjf`) into `third_party/cef/`.
