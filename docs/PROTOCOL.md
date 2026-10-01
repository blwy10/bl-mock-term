# MockTerm wire protocol

Spoken between `mtserver.exe` and `mtcomm.exe` (port 8195) and between
`mtcomm.exe` and its local clients (port 8194): `mockterm.exe`, `mtcli.exe`
and the integration tests. Implemented once, in `common/mtcore`
(`MessageCodec`, `Message`, `Element`) and `common/mtclient` (`Session`).
Loosely inspired by the shape of BLPAPI (services, requests, subscriptions,
correlation ids, element trees); it is not compatible with it.

## Framing

All integers are big-endian.

```
frame    := length:u32  payload            length = bytes in payload, <= 16 MiB
payload  := magic:u16 (0x4D54 "MT")  version:u8 (1)
            type:u16  correlation:u64  flags:u32
            service:string  operation:string  topic:string
            body:element
string   := length:u32  bytes (UTF-8)
element  := name:string  type:u8  value
value    := (none)                       type 0 NONE
          | u8 (0/1)                     type 1 BOOL
          | i64                          type 2 INT64, 5 DATETIME (epoch ms)
          | f64 (IEEE-754 bits as u64)   type 3 FLOAT64
          | string                       type 4 STRING
          | count:u32  element*count     type 6 SEQUENCE (named), 7 ARRAY (items)
```

Nesting is limited to 32 levels. A frame that fails to decode closes the
connection.

## Message types

| Value | Type | Direction | Notes |
| --- | --- | --- | --- |
| 1 / 2 | `LOGON` / `LOGON_ACK` | C -> S / S -> C | body `{user, application, conflateMs}`; ack body describes the server |
| 3 | `LOGOFF` | either | orderly close |
| 4 / 5 | `HEARTBEAT` / `HEARTBEAT_ACK` | either | idle sessions are dropped after the heartbeat timeout |
| 10 | `REQUEST` | C -> S | `service`, `operation`, body = arguments |
| 11 / 12 | `RESPONSE` / `PARTIAL_RESPONSE` | S -> C | same correlation id as the request |
| 13 | `REQUEST_FAILURE` | S -> C | body `{reason: {code, category, message}}` |
| 20 / 21 | `SUBSCRIBE` / `UNSUBSCRIBE` | C -> S | `service`, `topic`, body `{fields: [...]}`; correlation id names the subscription |
| 22 | `SUBSCRIPTION_STARTED` | S -> C | |
| 23 | `SUBSCRIPTION_FAILURE` | S -> C | body `{reason}` (e.g. unknown security) |
| 24 | `SUBSCRIPTION_DATA` | S -> C | body = field values; partial unless flagged as an image |
| 25 | `SUBSCRIPTION_TERMINATED` | S -> C | |
| 30 | `SESSION_STATUS` | S -> C | from `mtcomm`: body `{status: UPSTREAM_UP \| UPSTREAM_DOWN, ...}` |
| 31 | `SERVICE_STATUS` | S -> C | service up/down |

## Flags (`SUBSCRIPTION_DATA`)

| Bit | Flag | Meaning |
| --- | --- | --- |
| 0 | `INITIAL_PAINT` | full image; first message of a subscription |
| 1 | `RECAP` | full image re-sent after recovery; replaces the client's image |
| 2 | `STALE` | data may be out of date (served from cache while upstream is down) |
| 3 | `CONFLATED` | several upstream updates were merged into this one |

Clients merge partial updates into their image and replace it on
`INITIAL_PAINT` or `RECAP` (`host/marketdata/convert.cpp`).

## Services

| Service | Operations | Topics |
| --- | --- | --- |
| `//mt/mktdata` | `Snapshot {securities, fields}` | `<security>` e.g. `AAPL US Equity`; fields such as `LAST_PRICE BID ASK OPEN HIGH LOW PREV_CLOSE NET_CHANGE PCT_CHANGE VOLUME LAST_UPDATE_TIME SEQUENCE_NUMBER` |
| `//mt/refdata` | `SecurityList`, `SecuritySearch {query}`, `ReferenceData {securities, fields}`, `HistoricalData {security, range: 1D\|1M\|1Y}` -> `{bars}`, `FieldList` | |
| `//mt/news` | `Search {query, maxResults}` -> `{headlines}` | `HEADLINES` |
| `//mt/msg` | `Send {to, text}` -> `{message}` | `<user>`: messages for that user |
| `//mt/admin` | `Status`, `Services`, `Sessions` (served by `mtserver`) | |
| `//mtcomm/admin` | `Status` (served by `mtcomm`: upstream state, counters, cache, clients, topics) | |

## What mtcomm adds

`mtcomm` forwards requests upstream with its own correlation ids and maps the
replies back. Subscriptions with the same `service|topic|sorted fields` share
one upstream subscription; a late subscriber gets `SUBSCRIPTION_STARTED` and an
`INITIAL_PAINT` from the last-value cache without an upstream round trip. If
the client logged on with `conflateMs > 0`, partial updates are merged and sent
at most once per interval with `CONFLATED`. When the upstream drops, clients
stay connected, receive `SESSION_STATUS UPSTREAM_DOWN`, and cached images are
marked `STALE`; after reconnect the subscriptions are re-issued and the fresh
image arrives as `RECAP`.

## Trying it

```powershell
build\host\Release\mtcomm.exe --spawn-server          # mtcomm on 8194, mtserver on 8195
build\host\Release\mtcli.exe status
build\host\Release\mtcli.exe history "MSFT US Equity" 1M
build\host\Release\mtcli.exe subscribe "EURUSD Curncy" 5
```
