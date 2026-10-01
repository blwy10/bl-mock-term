#pragma once

// Process-message names exchanged between the renderer (window.mockterm) and
// the browser process. Arguments are documented per message.
namespace mockterm::platform::ipc {

// Renderer -> browser
inline constexpr char kSubscribe[] = "mt.subscribe";      // [int subId, list<string> tickers]
inline constexpr char kUnsubscribe[] = "mt.unsubscribe";  // [int subId]
inline constexpr char kRequest[] = "mt.request";          // [int reqId, string method, string a, string b]
inline constexpr char kListen[] = "mt.listen";            // [string channel]  ("news" | "chat")
inline constexpr char kUnlisten[] = "mt.unlisten";        // [string channel]
inline constexpr char kOpenApp[] = "mt.openApp";          // [string command]

// Browser -> renderer
inline constexpr char kTicks[] = "mt.ticks";        // [int subId, string json Tick[]]
inline constexpr char kResponse[] = "mt.response";  // [int reqId, bool ok, string json|error]
inline constexpr char kNews[] = "mt.news";          // [string json Headline]
inline constexpr char kChat[] = "mt.chat";          // [string json ChatMessage]

inline constexpr char kChannelNews[] = "news";
inline constexpr char kChannelChat[] = "chat";

// Request methods for kRequest.
inline constexpr char kMethodHistory[] = "history";        // a=ticker, b=range
inline constexpr char kMethodNews[] = "news";              // a=query
inline constexpr char kMethodChat[] = "chat";              // a=to, b=text
inline constexpr char kMethodSecurities[] = "securities";  // no args

// Custom scheme serving bundled apps: mockterm://apps/<app>/index.html
inline constexpr char kScheme[] = "mockterm";
inline constexpr char kAppsHost[] = "apps";
inline constexpr char kShellUrl[] = "mockterm://apps/shell/index.html";

}  // namespace mockterm::platform::ipc
