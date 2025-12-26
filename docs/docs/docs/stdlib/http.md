# `use` http

HTTP client for making HTTP and HTTPS requests.

## Functions

### `fn` get

Simple GET request.

**Signature:** `(url:str -- resp:Response)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `url` | `str` | The request URL (http:// or https://) |

| Output | Type | Description |
|--------|------|-------------|
| `resp` | `Response` | Response struct with status, headers, body |

**Example:**

```qd
"https://example.com" http::get! -> resp
resp @status print nl
resp http::close
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
"https://api.example.com" http::new -> req
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
req "Authorization" "Bearer token123" http::header
```

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

Set client certificate for mTLS (mutual TLS) authentication.

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

### `fn` send

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
req http::send! -> resp
resp @status print nl
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

## Structs

### Response

HTTP response data.

| Field | Type | Description |
|-------|------|-------------|
| `status` | `i64` | HTTP status code (200, 404, etc.) |
| `headers` | `str` | Response headers as string |
| `body` | `str` | Response body content |

---

## Error Codes

| Constant | Value | Description |
|----------|-------|-------------|
| `ErrConnect` | 2 | Connection failed |
| `ErrTls` | 3 | TLS/SSL error |
| `ErrTimeout` | 4 | Request timeout |
| `ErrParse` | 5 | Response parse error |
| `ErrInvalidUrl` | 6 | Invalid URL format |
| `ErrRedirect` | 7 | Too many redirects |
| `ErrMemory` | 8 | Memory allocation failed |
| `ErrSend` | 9 | Send failed |
| `ErrReceive` | 10 | Receive failed |

---

## Status Codes

### 2xx Success

| Constant | Value | Description |
|----------|-------|-------------|
| `StatusOK` | 200 | OK |
| `StatusCreated` | 201 | Created |
| `StatusAccepted` | 202 | Accepted |
| `StatusNoContent` | 204 | No Content |

### 3xx Redirection

| Constant | Value | Description |
|----------|-------|-------------|
| `StatusMovedPermanently` | 301 | Moved Permanently |
| `StatusFound` | 302 | Found |
| `StatusNotModified` | 304 | Not Modified |
| `StatusTemporaryRedirect` | 307 | Temporary Redirect |
| `StatusPermanentRedirect` | 308 | Permanent Redirect |

### 4xx Client Error

| Constant | Value | Description |
|----------|-------|-------------|
| `StatusBadRequest` | 400 | Bad Request |
| `StatusUnauthorized` | 401 | Unauthorized |
| `StatusForbidden` | 403 | Forbidden |
| `StatusNotFound` | 404 | Not Found |
| `StatusMethodNotAllowed` | 405 | Method Not Allowed |
| `StatusTooManyRequests` | 429 | Too Many Requests |

### 5xx Server Error

| Constant | Value | Description |
|----------|-------|-------------|
| `StatusInternalServerError` | 500 | Internal Server Error |
| `StatusBadGateway` | 502 | Bad Gateway |
| `StatusServiceUnavailable` | 503 | Service Unavailable |
| `StatusGatewayTimeout` | 504 | Gateway Timeout |

---

## Examples

### Simple GET Request

```qd
use http

fn main() {
	"https://example.com" http::get switch {
		Ok {
			-> resp
			"Status: " print resp @status print nl
			resp @body print nl
			resp http::close
		}
		_ {
			"Request failed" print nl
		}
	}
}
```

### POST with JSON

```qd
use http

fn main() {
	"https://api.example.com/data" http::new -> req
	req "POST" http::method
	req "Content-Type" "application/json" http::header
	req "{\"name\":\"test\"}" http::body

	req http::send switch {
		Ok {
			-> resp
			resp @status http::StatusCreated eq
			if {
				"Created successfully" print nl
			}
			resp http::close
		}
		_ {
			"Request failed" print nl
		}
	}

	req http::free_request
}
```

### Error Handling

```qd
use http

fn main() {
	"https://invalid.example" http::get switch {
		Ok {
			-> resp
			resp http::close
		}
		http::ErrConnect {
			"Connection failed" print nl
		}
		http::ErrTls {
			"TLS/SSL error" print nl
		}
		http::ErrTimeout {
			"Request timed out" print nl
		}
		_ {
			"Unknown error" print nl
		}
	}
}
```

### mTLS Request

```qd
use http

fn main() {
	// Create request to mTLS-protected endpoint
	"https://api.secure.example.com/data" http::new -> req

	// Set client certificate for mutual TLS authentication
	req "/path/client.crt" "/path/client.key" http::cert

	// Configure request
	req "GET" http::method
	req "Accept" "application/json" http::header

	// Send request (will use mTLS for TLS handshake)
	req http::send switch {
		Ok {
			-> resp
			"Status: " print resp @status print nl
			resp @body print nl
			resp http::close
		}
		http::ErrTls {
			"mTLS authentication failed" print nl
		}
		_ {
			"Request failed" print nl
		}
	}

	req http::free_request
}
```

---

# HTTP Server

The `http` module also provides a Gin-inspired HTTP server for building web applications. The server handles requests concurrently using thread-per-request.

## Server Functions

### `fn` engine

Create a new HTTP server engine.

**Signature:** `( -- engine:ptr)`

| Output | Type | Description |
|--------|------|-------------|
| `engine` | `ptr` | Server engine handle |

**Example:**

```qd
http::engine -> e
```

---

### `fn` GET, POST, PUT, DELETE, ANY

Register route handlers for specific HTTP methods.

**Signature:** `(engine:ptr path:str handler:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Server engine |
| `path` | `str` | Route path (supports `:param` for path parameters) |
| `handler` | `ptr` | Handler function pointer |

**Example:**

```qd
e "/" &handle_root http::GET
e "/users" &handle_users http::POST
e "/users/:id" &handle_user http::GET
```

---

### `fn` run

Start the HTTP server (blocking).

**Signature:** `(engine:ptr addr:str -- )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `engine` | `ptr` | Server engine |
| `addr` | `str` | Listen address (e.g., ":8080", "127.0.0.1:3000") |

**Example:**

```qd
e ":8080" http::run!
```

---

### `fn` free_engine

Free the server engine.

**Signature:** `(engine:ptr -- )`

---

## Handler Context

Handler functions receive a context pointer for accessing request data and sending responses.

### Request Methods

| Function | Signature | Description |
|----------|-----------|-------------|
| `http::param` | `(ctx:ptr name:str -- value:str)` | Get path parameter (e.g., `:id`) |
| `http::query_param` | `(ctx:ptr name:str -- value:str)` | Get query parameter |
| `http::get_header` | `(ctx:ptr name:str -- value:str)` | Get request header |
| `http::get_body` | `(ctx:ptr -- body:str)` | Get request body |
| `http::get_method` | `(ctx:ptr -- method:str)` | Get HTTP method |
| `http::get_path` | `(ctx:ptr -- path:str)` | Get request path |

### Response Methods

| Function | Signature | Description |
|----------|-----------|-------------|
| `http::string` | `(ctx:ptr status:i64 body:str -- )` | Send text response |
| `http::json` | `(ctx:ptr status:i64 body:str -- )` | Send JSON response |
| `http::html` | `(ctx:ptr status:i64 body:str -- )` | Send HTML response |
| `http::set_header` | `(ctx:ptr name:str value:str -- )` | Set response header |
| `http::status` | `(ctx:ptr code:i64 -- )` | Send status-only response |

---

## Route Groups

Group routes with a common prefix and shared middleware.

### `fn` group

Create a route group.

**Signature:** `(engine:ptr prefix:str -- group:ptr)`

**Example:**

```qd
e "/api" http::group -> api
api "/users" &handle_users http::group_GET
api "/posts" &handle_posts http::group_GET
```

### Group Route Methods

- `http::group_GET`
- `http::group_POST`
- `http::group_PUT`
- `http::group_DELETE`
- `http::group_ANY`

---

## Middleware

Add middleware functions that run before handlers.

### `fn` use

Add global middleware.

**Signature:** `(engine:ptr middleware:ptr -- )`

### `fn` group_use

Add middleware to a group.

**Signature:** `(group:ptr middleware:ptr -- )`

**Example:**

```qd
fn logger(c:ptr -- ) {
	-> c
	c http::get_method print " " print
	c http::get_path print nl
	c http::next
}

e &logger http::use
```

---

## Concurrency

The HTTP server uses **thread-per-request** concurrency. Each incoming request is handled in a separate thread with its own execution context. This allows the server to handle multiple simultaneous requests without blocking.

- Requests are handled concurrently (not sequentially)
- Each request gets an isolated execution context
- No changes needed in handler code - concurrency is automatic
- Thread cleanup is automatic

---

## Server Examples

### Basic Server

```qd
use http

fn handle_root(c:ptr -- ) {
	-> c
	c 200 "Hello, World!" http::string
}

fn main() {
	http::engine -> e
	e "/" &handle_root http::GET
	e ":8080" http::run!
	e http::free_engine
}
```

### REST API

```qd
use http

fn get_users(c:ptr -- ) {
	-> c
	c 200 "[{\"id\":1,\"name\":\"Alice\"}]" http::json
}

fn get_user(c:ptr -- ) {
	-> c
	c "id" http::param -> id
	c 200 id http::string
}

fn create_user(c:ptr -- ) {
	-> c
	c http::get_body -> body
	c 201 body http::json
}

fn main() {
	http::engine -> e

	e "/api" http::group -> api
	api "/users" &get_users http::group_GET
	api "/users/:id" &get_user http::group_GET
	api "/users" &create_user http::group_POST

	e ":8080" http::run!
	e http::free_engine
}
```

### With Middleware

```qd
use http

fn cors(c:ptr -- ) {
	-> c
	c "Access-Control-Allow-Origin" "*" http::set_header
	c http::next
}

fn handle_root(c:ptr -- ) {
	-> c
	c 200 "Hello!" http::string
}

fn main() {
	http::engine -> e
	e &cors http::use
	e "/" &handle_root http::GET
	e ":8080" http::run!
	e http::free_engine
}
```
