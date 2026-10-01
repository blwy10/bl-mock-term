# Architecture

MockTerm runs as three processes, loosely modelled on the public picture of the
Bloomberg Terminal: remote C++ server infrastructure, a local communications
process (`bbcomm.exe` on `localhost:8194`), and a Chromium-based terminal UI.
How Bloomberg actually splits its private services is not public; this is an
illustration, not a reverse-engineering.

```
 mtserver.exe   "remote" servers: feed handlers -> ticker plant -> services     (port 8195)
      |           //mt/mktdata  //mt/refdata  //mt/news  //mt/msg  //mt/admin
      |  binary framed protocol (docs/PROTOCOL.md), one TCP session
      v
 mtcomm.exe     local comms: subscription mux, last-value cache, conflation,   (port 8194)
      |           request routing, reconnect + recap, optional mtserver supervisor
      |  same protocol, one session per local client (mockterm, mtcli, tests)
      v
 mockterm.exe   CEF/Chromium host + TypeScript apps; md::CommsClient adapts the
                protocol to window.mockterm
```

By default `mockterm.exe` starts `mtcomm.exe --spawn-server` from its own
directory if nothing is listening on the comms port, and `mtcomm.exe` starts
and restarts `mtserver.exe`. Children are placed in a Win32 Job Object, so
closing the terminal tears down the chain. `--no-spawn-comms` attaches to an
already running `mtcomm.exe` instead (e.g. one started by hand with logging).

`mtserver` and `mtcomm` are deliberately written in an older, pre-C++11 style
(raw ownership, Win32 threads and critical sections, raw Winsock, hand-written
serialisation) and compiled as C++14, to give a realistic modernisation target.
See [MODERNISATION.md](MODERNISATION.md). The CEF host and `host/marketdata`
are modern C++20.

### mtserver.exe (`server/`)

| Component | Role |
| --- | --- |
| `FeedHandler` | One simulated exchange feed per venue: seeded random walk producing quote/trade packets with per-feed sequence numbers; a configurable fraction is dropped (`--drop-prob`) |
| `GapDetector` | Per-feed sequence tracking; counts gaps and duplicates |
| `TickerPlant` | Normalises packets into ticks, updates `TickCache` (last value per security) and `TimeSeries` (intraday bars), and publishes to subscribers |
| `SecurityMaster`, `FieldDictionary` | Reference data for equities, FX, indices and government yields; field metadata |
| `History` | Deterministic 1D/1M/1Y bars ending at the live price |
| `NewsEngine`, `ChatEngine` | Templated headlines; IB counterparties that reply quoting live prices |
| `Service` subclasses | `//mt/mktdata` (subscribe, `Snapshot`), `//mt/refdata` (`SecurityList`, `SecuritySearch`, `ReferenceData`, `HistoricalData`, `FieldList`), `//mt/news` (`Search`, topic `HEADLINES`), `//mt/msg` (`Send`, per-user topic), `//mt/admin` (`Status`, `Services`, `Sessions`) |
| `ClientSession`, `SubscriptionTable`, `Server` | Logon, heartbeats, correlation ids, fan-out on a worker thread pool |

### mtcomm.exe (`comms/`)

| Component | Role |
| --- | --- |
| `CommsEngine` | Owns the upstream `mtclient::Session`, the listener and local clients; routes requests by rewriting correlation ids; serves `//mtcomm/admin Status` |
| `TopicMux` | Normalises `service|topic|sorted fields`; equivalent local subscriptions share one upstream subscription |
| `LastValueCache` | Merged image per topic; a late subscriber gets an `INITIAL_PAINT` from cache; images go `STALE` when the upstream drops |
| `Conflator` | Per-client conflation interval (logon option `conflateMs`); merges partial updates, flags `CONFLATED` |
| Reconnect | `mtclient::Session` reconnects with exponential backoff; on recovery subscriptions are re-issued and the fresh image is fanned out with `RECAP` |
| `mtcli.exe` | Command-line client: `status`, `snapshot`, `refdata`, `history`, `news`, `search`, `send`, `subscribe`, ... |

### mockterm.exe (`host/`)

```
+------------------------------- mockterm.exe (browser process) -------------------------------+
|  main.cpp -> platform::RunMain                                                              |
|   MockTermApp (CefApp, CefBrowserProcessHandler)                                             |
|     - registers mockterm:// (standard, secure, CORS, fetch)                                  |
|     - AppSchemeHandlerFactory: mockterm://apps/<path> -> <exe dir>/apps/<path>               |
|     - creates one browser window -> mockterm://apps/shell/index.html                         |
|   MockTermClient (CefClient)  -- process messages -->  BrowserBridge                         |
|   BrowserBridge (md::CommsListener)                                                          |
|     - per-frame subscriptions / news+chat listeners, request dispatch                       |
|     - owns md::CommsClient (mtclient session to mtcomm), marshals callbacks to TID_UI       |
+-------------------------------------------^------------------------------------------------+
                                            | CefProcessMessage (JSON payloads)
+-------------------------------------------v--------- mockterm.exe --type=renderer ----------+
|   RendererBridge (CefRenderProcessHandler)                                                   |
|     - OnContextCreated: installs window.mockterm into mockterm:// frames only                |
|     - V8 handlers -> process messages; responses resolve CefV8Value promises                 |
|     - tick/news/chat messages invoke registered JS callbacks inside the frame's context     |
|                                                                                              |
|   shell (top-level page)  ->  <iframe> per app: des | gp | wei | top | ib                     |
+----------------------------------------------------------------------------------------------+
```

## Layers

| Layer | Path | Depends on CEF? |
| --- | --- | --- |
| Entry | `host/main.cpp` | No (calls `platform::RunMain`) |
| Platform | `host/platform/` | **Yes - the only place** |
| Market data client | `host/marketdata/` | No (C++20; protocol -> model conversion, unit + integration tested) |
| Protocol | `common/mtcore`, `common/mtclient` | No (legacy C++14) |
| Back end | `server/`, `comms/` | No (legacy C++14, separate processes) |
| Apps | `apps/src/` | No - only `window.mockterm` |

Keeping CEF in `platform/` means a CEF upgrade only touches those files, and the
whole data path can be tested without a browser.

## Processes

The same executable runs every CEF process. `RunMain` calls `CefExecuteProcess`;
sub-processes (renderer, GPU, utility) return from it and exit, the browser
process continues to `CefInitialize` / `CefRunMessageLoop`. Command-line
switches handled by the host:

| Switch | Meaning |
| --- | --- |
| `--remote-debugging-port=N` | enable DevTools/CDP (used by `scripts/e2e.ts`) |
| `--apps-dir=PATH` | serve apps from another directory (default `<exe dir>/apps`) |
| `--url=URL` | start page (default `mockterm://apps/shell/index.html`) |
| `--comms-port=N` | port of `mtcomm.exe` (default 8194) |
| `--server-port=N` | upstream port passed to a spawned `mtcomm.exe` (default 8195) |
| `--no-spawn-comms` | never start `mtcomm.exe`; wait for one to appear on the port |

The sandbox is disabled for simplicity (no `cef_sandbox.lib` link, no
`sandbox_info`). Enabling it requires linking the sandbox library with matching
CRT settings and passing `sandbox_info` to `CefExecuteProcess`/`CefInitialize`.

## Asset loading: `mockterm://`

`mockterm` is registered in `OnRegisterCustomSchemes` (both processes) as a
standard + secure scheme, so pages get a real origin (`mockterm://apps`),
same-origin iframes, `localStorage`, and ES module semantics. The handler maps
the URL path to `apps/` beside the executable, rejects `..` traversal, picks the
MIME type with `CefGetMimeType`, and returns a `CefStreamResourceHandler`.
There is no HTTP server and no network listener (other than the optional
DevTools port).

## Bridge protocol

`window.mockterm` (see `apps/src/types/mockterm.d.ts`):

```ts
subscribe(tickers, onTick) => unsubscribe   // streaming, <= 10 Hz per subscription
getHistory(ticker, '1D'|'1M'|'1Y') => Promise<Bar[]>
getNews(query?) => Promise<Headline[]>
onNews(cb) => unsubscribe
sendChat(to, text) => Promise<void>
onChat(cb) => unsubscribe
openApp(command)                            // asks the shell to run a command
getSecurities() => Promise<SecurityInfo[]>  // extension: universe for autocomplete/DES
```

Messages (`host/platform/ipc.h`):

| Direction | Name | Args |
| --- | --- | --- |
| R -> B | `mt.subscribe` | js id, list of tickers |
| R -> B | `mt.unsubscribe` | js id |
| R -> B | `mt.listen` / `mt.unlisten` | channel (`news` / `chat`) |
| R -> B | `mt.request` | request id, method (`history`/`news`/`chat`/`securities`), a, b |
| R -> B | `mt.openApp` | command |
| B -> R | `mt.ticks` | js id, JSON array of ticks |
| B -> R | `mt.news` / `mt.chat` | JSON object |
| B -> R | `mt.response` | request id, ok, JSON result or error string |

Payloads are JSON strings produced by `host/marketdata/json.cpp` and parsed with
the page's own `JSON.parse` in the renderer. Request and subscription ids are
unique per renderer process. State is tracked per frame and per V8 context
(DevTools can add isolated worlds to a frame); releasing a context tears down
its subscriptions, and detaching a frame drops all of its browser-side state.

`openApp` is routed through the browser: it dispatches a `mockterm:open`
`CustomEvent` on the top-level shell, which parses and opens the command, so an
app inside an iframe can open another app without knowing about the shell.

## Market data client

`md::CommsClient` (`host/marketdata/comms_client.*`) holds one reconnecting
`mtclient::Session` to `mtcomm` (logon user `ME`, `conflateMs=100`, i.e. at
most 10 Hz per subscription):

| `window.mockterm` | Wire |
| --- | --- |
| `subscribe(tickers)` | one `//mt/mktdata` subscription per ticker; `INITIAL_PAINT`/`RECAP` replace the cached tick, partial updates merge into it (`convert.cpp`) |
| `getHistory(t, range)` | `//mt/refdata HistoricalData {security, range}` |
| `getSecurities()` | `//mt/refdata SecurityList` |
| `getNews(query)` / `onNews` | `//mt/news Search` / subscription to `HEADLINES` |
| `sendChat` / `onChat` | `//mt/msg Send` / subscription to the user's topic |

Session events become a connection state (`connecting`, `live`, `stale` when
`mtcomm` reports the upstream down, `down` when `mtcomm` itself is gone), which
`BrowserBridge` dispatches to the shell as a `mockterm:status` event and the
shell renders in the top bar. If a spawned `mtcomm.exe` exits, the client
restarts it.

Callbacks fire on the session's reader thread; `BrowserBridge` serialises to
JSON there and posts the send to the CEF UI thread with `CefPostTask(TID_UI)`.

## Apps

Each app is a small TypeScript entry point bundled by esbuild into
`apps/dist/<app>/main.js` with its HTML/CSS. The shell owns the command line,
autocomplete, history (`localStorage`), tabs/tiling and the ticker strip, and
hosts each screen in an iframe (`mockterm://apps/<app>/index.html?sec=...`).
Command parsing is pure (`apps/src/shell/command.ts`) and unit tested.
