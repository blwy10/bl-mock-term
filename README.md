# MockTerm

A Windows desktop market terminal **mock**: a native C++ host that embeds
Chromium via the [Chromium Embedded Framework (CEF)](https://bitbucket.org/chromiumembedded/cef),
a set of dense, keyboard-driven TypeScript "terminal apps" rendered inside it,
and a separate C++ back end loosely modelled on the public picture of the
Bloomberg Terminal's architecture. All data is simulated; no real market data
or third-party branding is used.

```
mtserver.exe   simulated remote servers: feeds, ticker plant, refdata, history, news, IB   :8195
     |  binary protocol (docs/PROTOCOL.md)
mtcomm.exe     local comms process (cf. bbcomm.exe): sharing, cache, conflation, reconnect :8194
     |  same protocol
mockterm.exe   CEF/Chromium/V8 host + TypeScript apps
```

`mtserver` and `mtcomm` are written in a deliberately older C++ style as a
modernisation target; see [docs/MODERNISATION.md](docs/MODERNISATION.md).

![MockTerm tiled layout](docs/screenshots/6-tiled.png)

| Item | Version |
| --- | --- |
| CEF | `154.0.25+g1bc6331+chromium-154.0.8037.44` (Standard Distribution, windows64) |
| Chromium | `154.0.8037.44` |
| Toolchain | MSVC 14.44 (Visual Studio 2022 Build Tools), CMake >= 3.21, Node >= 20 |

The CEF version is pinned in exactly one place: `CEF_VERSION` in
[`cmake/cef.cmake`](cmake/cef.cmake). See [docs/UPGRADING.md](docs/UPGRADING.md).

## Quick start

```powershell
# Prereqs: Visual Studio 2022 (or Build Tools) with "Desktop development with C++",
#          Node.js 20+, PowerShell 5+. CMake is taken from PATH or the VS install.
scripts\build.ps1                      # fetch CEF, build apps, CMake Release x64, ctest
scripts\run.ps1                        # launch build\host\Release\mockterm.exe
scripts\run.ps1 --remote-debugging-port=9222   # with DevTools / CDP enabled
```

`scripts\build.ps1` options: `-Clean` (wipe `build/`), `-Config Debug`, `-SkipTests`.
`scripts\get-cef.ps1` downloads and SHA1-verifies the pinned CEF distribution
into `third_party/cef/` (about 300 MB compressed); `build.ps1` calls it automatically.

The runtime is self-contained in `build\host\Release\`: `mockterm.exe`,
`mtcomm.exe`, `mtserver.exe`, `mtcli.exe`, the CEF binaries/resources, and the
bundled apps under `apps\`. `mockterm.exe` starts `mtcomm.exe`, which starts
and supervises `mtserver.exe`; the top bar shows `LIVE`, `STALE` (server
unreachable, cached data) or `DISCONNECTED`. To run the back end yourself:

```powershell
build\host\Release\mtcomm.exe --spawn-server --log-level debug
build\host\Release\mockterm.exe --no-spawn-comms
build\host\Release\mtcli.exe status                 # counters, cache, topics, clients
build\host\Release\mtcli.exe subscribe "AAPL US Equity" 5
```

## Using the terminal

Type into the command line at the top and press **Enter** (`<GO>`):

| Command | Screen |
| --- | --- |
| `AAPL US Equity DES` | Security description, live quote, fundamentals, related news |
| `EURUSD Curncy GP` | Price chart (1D / 1M / 1Y) with live last price |
| `WEI` | World equity indices monitor, grouped by region, live ticks |
| `TOP` | Top news with live headlines and story view (`AAPL US Equity TOP` filters) |
| `IB` | Instant messaging with simulated counterparties |

Commands are case-insensitive; short tickers resolve (`aapl gp`), and a bare
security opens `DES`. Keys: **Tab** autocompletes, **Up/Down** recall history
(persisted), **F2** toggles tiled panels, **Ctrl+W** closes the active panel,
**Esc** clears. Double-click a WEI row to chart it; ticker links in TOP open DES.

## Repository layout

```
cmake/cef.cmake          CEF version pin + locator
host/main.cpp            wWinMain -> platform::RunMain
host/platform/           ALL direct CEF usage: process entry, CefApp/CefClient,
                         mockterm:// scheme handler, browser + renderer bridge
host/marketdata/         CEF-free mtcomm client, protocol -> model conversion, JSON
host/tests/              GoogleTest unit + integration tests (ctest)
common/mtcore/           legacy C++: threads, sockets, codec, element, logging, process
common/mtclient/         legacy C++: reconnecting protocol client session
server/                  legacy C++: mtserver.exe
comms/                   legacy C++: mtcomm.exe, mtcli.exe
cmake/legacy.cmake       C++14 settings for the legacy targets
apps/src/                TypeScript apps: shell, des, gp, wei, top, ib, common
apps/src/types/mockterm.d.ts   the window.mockterm contract
apps/test/               Vitest tests (command parser, autocomplete, history)
scripts/                 get-cef.ps1, build.ps1, run.ps1, e2e.ts (Playwright over CDP)
docs/                    ARCHITECTURE.md, PROTOCOL.md, MODERNISATION.md, UPGRADING.md
```

## Tests

```powershell
cd apps; npm test                                   # Vitest
ctest --test-dir build -C Release --output-on-failure   # GoogleTest (run by build.ps1)
cd scripts; npm ci; npm run e2e                     # launches mockterm.exe with
                                                    # --remote-debugging-port=9222
```

GoogleTest covers the codec, element model, gap detection, cache, conflation
and subscription sharing; an in-process server + comms test for sharing,
outage and recap; and `CommsClient` against real `mtcomm.exe`/`mtserver.exe`
processes.

The end-to-end test attaches Playwright via CDP, checks the top bar goes
`LIVE`, kills `mtserver.exe` and waits for `STALE` and then `LIVE` again (mtcomm
restarts the server), opens every screen from the
command line, asserts live ticks change the DOM over several seconds, sends an
IB message and waits for the reply, and writes screenshots to
`artifacts/screenshots/`. Use `npm run e2e -- --attach` to drive an instance
that is already running.

## Screenshots

| | |
| --- | --- |
| ![DES](docs/screenshots/1-des.png) | ![WEI](docs/screenshots/2-wei.png) |
| ![GP](docs/screenshots/5-gp.png) | ![TOP](docs/screenshots/3-top.png) |
| ![IB](docs/screenshots/4-ib.png) | ![Launchpad](docs/screenshots/0-launchpad.png) |

## Limitations

- CEF runs without the sandbox (`USE_SANDBOX OFF`, `no_sandbox = true`) to keep
  the build simple; see ARCHITECTURE.md.
- Windows x64 only.
- All prices, news and chat are synthetic and deterministic per seed.
- The back-end protocol is unauthenticated and listens on loopback only.
