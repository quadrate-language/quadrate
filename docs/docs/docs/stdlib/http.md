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
