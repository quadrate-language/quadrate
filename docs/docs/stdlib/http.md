# `use` http

HTTP module.
Provides HTTP client and server functionality.
Error codes: Ok=1 (success), specific errors start at 2

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `ErrAccept` | `21` | Server error: accept failed. |
| `ErrBind` | `20` | Server error: bind failed. |
| `ErrConnect` | `2` | Connection failed. |
| `ErrInvalidUrl` | `6` | Invalid URL format. |
| `ErrMemory` | `8` | Memory allocation failed. |
| `ErrParse` | `5` | Response parse error. |
| `ErrReceive` | `10` | Receive failed. |
| `ErrRedirect` | `7` | Too many redirects. |
| `ErrSend` | `9` | Send failed. |
| `ErrServerParse` | `22` | Server error: parse failed. |
| `ErrTimeout` | `4` | Request timeout. |
| `ErrTls` | `3` | TLS handshake failed. |
| `StatusAccepted` | `202` |  |
| `StatusAlreadyReported` | `208` |  |
| `StatusBadGateway` | `502` |  |
| `StatusBadRequest` | `400` |  |
| `StatusConflict` | `409` |  |
| `StatusContinue` | `100` |  |
| `StatusCreated` | `201` |  |
| `StatusEarlyHints` | `103` |  |
| `StatusExpectationFailed` | `417` |  |
| `StatusFailedDependency` | `424` |  |
| `StatusForbidden` | `403` |  |
| `StatusFound` | `302` |  |
| `StatusGatewayTimeout` | `504` |  |
| `StatusGone` | `410` |  |
| `StatusHTTPVersionNotSupported` | `505` |  |
| `StatusIMUsed` | `226` |  |
| `StatusInsufficientStorage` | `507` |  |
| `StatusInternalServerError` | `500` |  |
| `StatusLengthRequired` | `411` |  |
| `StatusLocked` | `423` |  |
| `StatusLoopDetected` | `508` |  |
| `StatusMethodNotAllowed` | `405` |  |
| `StatusMisdirectedRequest` | `421` |  |
| `StatusMovedPermanently` | `301` |  |
| `StatusMultipleChoices` | `300` |  |
| `StatusMultiStatus` | `207` |  |
| `StatusNetworkAuthRequired` | `511` |  |
| `StatusNoContent` | `204` |  |
| `StatusNonAuthoritativeInfo` | `203` |  |
| `StatusNotAcceptable` | `406` |  |
| `StatusNotExtended` | `510` |  |
| `StatusNotFound` | `404` |  |
| `StatusNotImplemented` | `501` |  |
| `StatusNotModified` | `304` |  |
| `StatusOK` | `200` |  |
| `StatusPartialContent` | `206` |  |
| `StatusPayloadTooLarge` | `413` |  |
| `StatusPaymentRequired` | `402` |  |
| `StatusPermanentRedirect` | `308` |  |
| `StatusPreconditionFailed` | `412` |  |
| `StatusPreconditionRequired` | `428` |  |
| `StatusProcessing` | `102` |  |
| `StatusProxyAuthRequired` | `407` |  |
| `StatusRangeNotSatisfiable` | `416` |  |
| `StatusRequestHeaderFieldsTooLarge` | `431` |  |
| `StatusRequestTimeout` | `408` |  |
| `StatusResetContent` | `205` |  |
| `StatusSeeOther` | `303` |  |
| `StatusServiceUnavailable` | `503` |  |
| `StatusSwitchingProtocols` | `101` |  |
| `StatusTeapot` | `418` |  |
| `StatusTemporaryRedirect` | `307` |  |
| `StatusTooEarly` | `425` |  |
| `StatusTooManyRequests` | `429` |  |
| `StatusUnauthorized` | `401` |  |
| `StatusUnavailableForLegalReasons` | `451` |  |
| `StatusUnprocessableEntity` | `422` |  |
| `StatusUnsupportedMediaType` | `415` |  |
| `StatusUpgradeRequired` | `426` |  |
| `StatusURITooLong` | `414` |  |
| `StatusUseProxy` | `305` |  |
| `StatusVariantAlsoNegotiates` | `506` |  |

## Functions

### `fn` abort

Abort request processing and send error response. Prevents further middleware/handlers from running.

**Signature:** `(c:Ctx status:i64 body:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |
| `status` | `i64` | HTTP status code |
| `body` | `str` | Error message |

**Example:**

```qd
c 401 "Unauthorized" http::abort
```
---

### `fn` ANY

Register handler for any HTTP method.

**Signature:** `(engine:ptr path:str handler:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Engine |
| `path` | `str` | Route path |
| `handler` | `ptr` | Handler function |
---

### `fn` body

Set request body.

**Signature:** `(req:ptr body:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `req` | `ptr` | Request object |
| `body` | `str` | Request body content |

**Example:**

```qd
req "{\"key\":\"value\"}" http::body
```
---

### `fn` cert

Set client certificate for mTLS (mutual TLS).

**Signature:** `(req:ptr cert_path:str key_path:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `req` | `ptr` | Request object |
| `cert_path` | `str` | Path to PEM client certificate file |
| `key_path` | `str` | Path to PEM client private key file |

**Example:**

```qd
req "/path/client.crt" "/path/client.key" http::cert
```
---

### `fn` close

Close and free response.

**Signature:** `(resp:Response -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `resp` | `Response` | Response object |

**Example:**

```qd
resp http::close
```
---

### `fn` DELETE

Register DELETE route.

**Signature:** `(engine:ptr path:str handler:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Engine |
| `path` | `str` | Route path |
| `handler` | `ptr` | Handler function |
---

### `fn` engine

Create new HTTP engine.

**Signature:** `( -- engine:ptr)`

| Output | Type | Description |
|--------|------|-------------|
| `engine` | `ptr` | Engine handle |

**Example:**

```qd
http::engine  // e
```
---

### `fn` free_engine

Free engine resources.

**Signature:** `(engine:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Engine |

**Example:**

```qd
e http::free_engine
```
---

### `fn` free_request

Free request object (call after send).

**Signature:** `(req:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `req` | `ptr` | Request object |

**Example:**

```qd
req http::free_request
```
---

### `fn` GET

Register GET route.   - :param for path parameters (e.g., "/users/:id")   - /* for wildcard suffix (e.g., "/static/*" matches /static/anything)

**Signature:** `(engine:ptr path:str handler:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Engine |
| `path` | `str` | Route path. Supports |
| `handler` | `ptr` | Handler function receiving Ctx |

**Example:**

```qd
e "/" fn(c:Ctx -- ) { c 200 "Hello" http::string } http::GET
```
---

### `fn` get_header

Get header value by name.

**Signature:** `(c:Ctx name:str -- value:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |
| `name` | `str` | Header name (case-insensitive) |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Header value (empty if not found) |

**Example:**

```qd
c "Content-Type" http::get_header  // ct
```
---

### `fn` group_DELETE

Register DELETE on group.

**Signature:** `(group:ptr path:str handler:ptr -- )`
---

### `fn` group

Create route group with prefix.

**Signature:** `(engine:ptr prefix:str -- group:ptr)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Engine |
| `prefix` | `str` | Path prefix (e.g., "/api") |

| Output | Type | Description |
|--------|------|-------------|
| `group` | `ptr` | Group handle |

**Example:**

```qd
e "/api" http::group  // api
```
---

### `fn` group_GET

Register GET on group.

**Signature:** `(group:ptr path:str handler:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `group` | `ptr` | Group handle |
| `path` | `str` | Route path (relative to group prefix) |
| `handler` | `ptr` | Handler function |

**Example:**

```qd
api "/users" fn(c:Ctx -- ) { ... } http::group_GET
```
---

### `fn` group_POST

Register POST on group.

**Signature:** `(group:ptr path:str handler:ptr -- )`
---

### `fn` group_PUT

Register PUT on group.

**Signature:** `(group:ptr path:str handler:ptr -- )`
---

### `fn` group_use

Add middleware to group.

**Signature:** `(group:ptr middleware:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `group` | `ptr` | Group handle |
| `middleware` | `ptr` | Middleware function |

**Example:**

```qd
api fn(c:Ctx -- ) { ... } http::group_use
```
---

### `fn` handle_one

Handle a single request (non-blocking alternative to run). Returns 0 if no request pending, 1 if request handled.

**Signature:** `(engine:ptr -- handled:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Engine |

| Output | Type | Description |
|--------|------|-------------|
| `handled` | `i64` | 1 if request was handled, 0 otherwise |

**Example:**

```qd
e http::handle_one  // handled
```
---

### `fn` header

Add request header.

**Signature:** `(req:ptr name:str value:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `req` | `ptr` | Request object |
| `name` | `str` | Header name |
| `value` | `str` | Header value |

**Example:**

```qd
req "Content-Type" "application/json" http::header
```
---

### `fn` html

Send HTML response (sets Content-Type: text/html).

**Signature:** `(c:Ctx status:i64 body:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |
| `status` | `i64` | HTTP status code |
| `body` | `str` | HTML body |

**Example:**

```qd
c 200 "<h1>Hello</h1>" http::html
```
---

### `fn` is_responded

Check if response has already been sent.

**Signature:** `(c:Ctx -- sent:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |

| Output | Type | Description |
|--------|------|-------------|
| `sent` | `i64` | 1 if sent, 0 otherwise |

**Example:**

```qd
c http::is_responded  // sent
```
---

### `fn` json

Send JSON response (sets Content-Type: application/json).

**Signature:** `(c:Ctx status:i64 body:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |
| `status` | `i64` | HTTP status code |
| `body` | `str` | JSON body |

**Example:**

```qd
c 200 "{\"ok\":true}" http::json
```
---

### `fn` method

Set request HTTP method.

**Signature:** `(req:ptr method:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `req` | `ptr` | Request object |
| `method` | `str` | HTTP method (GET, POST, PUT, DELETE, etc.) |

**Example:**

```qd
req "POST" http::method
```
---

### `fn` new

Create new HTTP request builder.

**Signature:** `(url:str -- req:ptr)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `url` | `str` | The request URL (http:// or https://) |

| Output | Type | Description |
|--------|------|-------------|
| `req` | `ptr` | Request object |

**Example:**

```qd
"https://example.com" http::new  // req
```
---

### `fn` param

Get path parameter by name.

**Signature:** `(c:Ctx name:str -- value:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |
| `name` | `str` | Parameter name (e.g., "id" for route "/users/:id") |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Parameter value |

**Example:**

```qd
c "id" http::param  // id
```
---

### `fn` POST

Register POST route.

**Signature:** `(engine:ptr path:str handler:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Engine |
| `path` | `str` | Route path |
| `handler` | `ptr` | Handler function |

**Example:**

```qd
e "/api/users" fn(c:Ctx -- ) { ... } http::POST
```
---

### `fn` PUT

Register PUT route.

**Signature:** `(engine:ptr path:str handler:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Engine |
| `path` | `str` | Route path |
| `handler` | `ptr` | Handler function |
---

### `fn` query_param

Get query parameter by name.

**Signature:** `(c:Ctx name:str -- value:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |
| `name` | `str` | Parameter name |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Parameter value (empty if not found) |

**Example:**

```qd
c "page" http::query_param  // page
```
---

### `fn` request_body

Get the raw request body as a string. Useful for reading JSON payloads in POST/PUT handlers.

**Signature:** `(c:Ctx -- body:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |

| Output | Type | Description |
|--------|------|-------------|
| `body` | `str` | Request body (empty if no body) |

**Example:**

```qd
c http::request_body  // json_payload
```
---

### `fn` run

Start server (blocking).

**Signature:** `(engine:ptr addr:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Engine |
| `addr` | `str` | Address like ":8080" or "127.0.0.1:3000" |

| Error | Description |
|-------|-------------|
| `http::ErrBind` | Failed to bind to address |

**Example:**

```qd
e ":8080" http::run!
```
---

### `fn` set_header

Set response header. Must be called before sending response body.

**Signature:** `(c:Ctx name:str value:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |
| `name` | `str` | Header name |
| `value` | `str` | Header value |

**Example:**

```qd
c "X-Custom" "value" http::set_header
```
---

### `fn` sse_end

End the SSE stream. Marks the stream as complete. The connection will be closed after the handler returns.

**Signature:** `(c:Ctx -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |

**Example:**

```qd
c http::sse_end
```
---

### `fn` sse_event

Send a named SSE event. The event is sent as "event: <name>\ndata: <content>\n\n". Clients can listen for specific event types with addEventListener.

**Signature:** `(c:Ctx name:str data:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |
| `name` | `str` | Event name |
| `data` | `str` | Event data (typically JSON) |

**Example:**

```qd
c "progress" "{\"percent\": 50}" http::sse_event
```
---

### `fn` sse_send

Send a data-only SSE event. The data is sent as "data: <content>\n\n".

**Signature:** `(c:Ctx data:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |
| `data` | `str` | Event data (typically JSON) |

**Example:**

```qd
c "{\"count\": 1}" http::sse_send
```
---

### `fn` sse_start

Start an SSE (Server-Sent Events) stream. Sends SSE headers and keeps the connection open for streaming events. Must be called before sse_send or sse_event.

**Signature:** `(c:Ctx -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |

**Example:**

```qd
c http::sse_start
```
---

### `fn` static

Serve files from a directory. Maps URL paths to filesystem paths. Use with a wildcard route (/*). Automatically serves index.html for directory requests. Returns 404 if file not found, 403 for path traversal attempts. // Serve files from ./public at /static/* fn handle_static(c:ptr -- ) {     -> c     c "/static" "./public" http::static } e "/static/*" &handle_static http::GET

**Signature:** `(c:Ctx prefix:str fs_path:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |
| `prefix` | `str` | URL prefix to strip (e.g., "/static") |
| `fs_path` | `str` | Filesystem directory path (e.g., "./public") |
---

### `fn` static_file

Serve a single file from the filesystem. Automatically detects MIME type from file extension. Returns 404 if file not found, 403 for path traversal attempts.

**Signature:** `(c:Ctx filepath:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |
| `filepath` | `str` | Path to file on filesystem |

**Example:**

```qd
c "/var/www/index.html" http::static_file
```
---

### `fn` stop

Stop the server.

**Signature:** `(engine:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Engine |

**Example:**

```qd
e http::stop
```
---

### `fn` string

Send plain text response.

**Signature:** `(c:Ctx status:i64 body:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `c` | `Ctx` | Context |
| `status` | `i64` | HTTP status code |
| `body` | `str` | Response body |

**Example:**

```qd
c 200 "Hello, World!" http::string
```
---

### `fn` use

Add middleware to engine. Middleware runs before route handlers on every request.

**Signature:** `(engine:ptr middleware:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Engine |
| `middleware` | `ptr` | Middleware function |

**Example:**

```qd
e fn(c:Ctx -- ) { c "X-Server" "Quadrate" http::set_header } http::use
```
## Ctx

HTTP Context structure (for server handlers). Passed to route handlers, contains request data and response methods. Fields:   method - GET, POST, PUT, DELETE, etc.   path - /users/123 (path without query string)   query - Raw query string (foo=bar&baz=qux)   headers - Request headers   body - Request body   params - Path parameters as "key=val\nkey2=val2"   _socket - Internal: client socket   _responded - Internal: response sent flag

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `method` | `str` |  |
| `path` | `str` |  |
| `query` | `str` |  |
| `headers` | `str` |  |
| `body` | `str` |  |
| `params` | `str` |  |
| `_socket` | `i64` |  |
| `_responded` | `i64` |  |

## Response

HTTP Response structure (for client)

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `status` | `i64` |  |
| `headers` | `str` |  |
| `body` | `str` |  |

### Constructors

#### `fn` get

Simple GET request.

**Signature:** `(url:str -- resp:Response)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `url` | `str` | The request URL |

| Output | Type | Description |
|--------|------|-------------|
| `resp` | `Response` | Response struct |

**Example:**

```qd
"https://example.com" http::get!  // resp
```
---

#### `fn` send

Execute HTTP request.

**Signature:** `(req:ptr -- resp:Response)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `req` | `ptr` | Request object |

| Output | Type | Description |
|--------|------|-------------|
| `resp` | `Response` | Response struct with status, headers, body |

**Example:**

```qd
req http::send!  // resp  resp <<status print
```

