# Architecture

```
+------------------------------- mockterm.exe (browser process) -------------------------------+
|  main.cpp -> platform::RunMain                                                              |
|   MockTermApp (CefApp, CefBrowserProcessHandler)                                             |
|     - registers mockterm:// (standard, secure, CORS, fetch)                                  |
|     - AppSchemeHandlerFactory: mockterm://apps/<path> -> <exe dir>/apps/<path>               |
|     - creates one browser window -> mockterm://apps/shell/index.html                         |
|   MockTermClient (CefClient)  -- process messages -->  BrowserBridge                         |
|   BrowserBridge (md::MarketDataListener)                                                     |
|     - per-frame subscriptions / news+chat listeners, request dispatch                       |
|     - owns md::MarketDataService (background thread) and marshals callbacks to TID_UI       |
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
| Market data | `host/marketdata/` | No (pure C++20, unit tested) |
| Apps | `apps/src/` | No - only `window.mockterm` |

Keeping CEF in `platform/` means a CEF upgrade only touches those files, and the
simulator can be tested without a browser.

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

## Market data

`MarketDataService` runs a 25 ms loop on its own thread:

- `Simulator`: 38 instruments (equities, FX, indices, government yields) with a
  seeded random walk, bid/ask spread, OHLC, volume, change, timestamps and a
  monotonically increasing sequence number.
- `SubscriptionManager`: per-subscriber ticker sets; ticks are coalesced to the
  latest value per ticker and flushed at most every 100 ms (10 Hz). A new
  subscription receives an immediate snapshot.
- `NewsGenerator`: backfills an archive and emits a headline every 2.5-6 s,
  templated against current prices.
- `ChatBot`: replies to IB messages after a short delay, quoting live prices for
  tickers mentioned in the message.
- `History`: deterministic bars ending at the current price for 1D/1M/1Y.

Listener callbacks fire on the service thread; `BrowserBridge` serialises to
JSON there and posts the send to the CEF UI thread with `CefPostTask(TID_UI)`.

## Apps

Each app is a small TypeScript entry point bundled by esbuild into
`apps/dist/<app>/main.js` with its HTML/CSS. The shell owns the command line,
autocomplete, history (`localStorage`), tabs/tiling and the ticker strip, and
hosts each screen in an iframe (`mockterm://apps/<app>/index.html?sec=...`).
Command parsing is pure (`apps/src/shell/command.ts`) and unit tested.
