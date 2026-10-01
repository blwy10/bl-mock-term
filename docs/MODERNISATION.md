# Modernisation inventory

`common/mtcore`, `common/mtclient`, `server/` and `comms/` (about 12k lines)
are written on purpose in a pre-C++11 style and built as C++14 with
`_HAS_AUTO_PTR_ETC=1` (`cmake/legacy.cmake`). They work and are tested; the
style is the point. This file lists what a modernisation would change. The CEF
host (`host/platform`) and `host/marketdata` are already C++20 and out of scope.

| Area | Lines |
| --- | --- |
| `common/mtcore` (threads, sockets, codec, element, logging, process) | ~3.7k |
| `common/mtclient` (reconnecting client session) | ~0.7k |
| `server/` (`mtserver.exe`) | ~5.2k |
| `comms/` (`mtcomm.exe`, `mtcli.exe`) | ~2.5k |

## Legacy idioms and their modern replacements

| Legacy idiom (approx. occurrences) | Where | Modern replacement |
| --- | --- | --- |
| Raw `new`/`delete`, ownership flags, `std::auto_ptr` (~35 / ~27 / 5) | `Thread`, `ThreadPool`, `Server`, `CommsEngine`, `TopicMux`, `LastValueCache` | `std::unique_ptr`, `std::make_unique`, values in containers |
| Intrusive ref counting (`RefCounted`, `Handle<T>` in `mtcore_refcount.h`) | `LocalClientHandle`, connections | `std::shared_ptr` / `weak_ptr` |
| `CRITICAL_SECTION` `Mutex` + hand-written `LockGuard` (~130 uses) | everywhere | `std::mutex`, `std::scoped_lock`, `std::shared_mutex` for read-mostly maps |
| Win32 `Event`, `WaitForSingleObject` | `Thread`, `CommsEngine` timers, `Session` | `std::condition_variable`, `std::stop_token`, `std::latch` |
| `_beginthreadex` + `Runnable`/`MemberRunnable<T>` | `mtcore_thread.*`, thread pool | `std::jthread` with lambdas |
| `Interlocked*` on `volatile` counters (~33) | stats and counters | `std::atomic<int64_t>` |
| `typedef` integer aliases (`Int64`, `Uint32`, ~44 typedefs) | `mtcore_platform.h` | `<cstdint>` and `using` |
| `struct X { enum Value {...}; }` pseudo-scoped enums | `MessageType`, `MessageFlags`, `DataType`, `SessionState` | `enum class`, flag types with operators |
| `Element` type-tag + union-ish storage | `mtcore_element.*` | `std::variant`, visitation |
| Sentinel return codes (`0` / non-zero), out-parameters (`get(Element *out, ...)`) | codec, caches, sockets | `std::optional`, `std::expected`, return values |
| `const std::string&` / `const char *` everywhere | parsing, lookups | `std::string_view` |
| `snprintf`, `char buf[...]` formatting (~22 / ~9) | `mtcore_log`, `mtcore_time`, stats | `std::format` |
| Epoch-millisecond `Int64` time | `TimeUtil`, timers, bars | `std::chrono` durations and `time_point` |
| Explicit `::iterator` loops (~53), hand-written functors | containers throughout | range-for, `<algorithm>`, `<ranges>`, lambdas |
| `NULL` (~99) | everywhere | `nullptr` |
| Raw Winsock, blocking reader thread per connection | `mtcore_socket`, `mtcore_connection` | RAII socket type; Asio / IOCP; coroutines for the session state machines |
| Hand-rolled big-endian `ByteWriter`/`ByteReader` | `mtcore_codec.cpp` | `std::span`, `std::byteswap`/`std::endian` |
| `strcmp` argument parsing | `mtsrv_config.cpp`, `mtcomm_config.cpp` | table-driven parser over `std::span<char*>` |
| Virtual-dispatch `Service` registry with string switch | `mtsrv_service.*`, `mtsrv_refdataservice.cpp` | handler map of `std::function`, or `std::variant` of request types |
| Win32 process + Job Object wrapper | `mtcore_process.*` | RAII handle types (`unique_handle`) |


## Suggested order

1. `nullptr`, `using`/`<cstdint>`, `enum class`, `override` (none today): mechanical, no behaviour change.
2. Ownership: `unique_ptr`/`shared_ptr`, remove `RefCounted`/`Handle` and `auto_ptr`; drop `_HAS_AUTO_PTR_ETC`.
3. Concurrency: `std::jthread`, `std::mutex`, `std::atomic`, `std::condition_variable`.
4. Types: `std::chrono`, `std::string_view`, `std::optional`, `std::variant` for `Element`.
5. Formatting and algorithms: `std::format`, ranges, lambdas.
6. I/O: coroutine-based sessions over an async socket layer.
7. Raise `cmake/legacy.cmake` from C++14 to C++20 and enable `/W4 /permissive-`.

The safety net is `host/tests/backend_unit_test.cpp` (codec, element, gap
detector, cache, conflator, mux), `backend_integration_test.cpp` (in-process
server + comms: sharing, cache paint, outage, recap) and
`comms_client_test.cpp` (real `mtcomm.exe`/`mtserver.exe` processes), plus the
Playwright E2E, which kills `mtserver.exe` and checks the terminal recovers.
