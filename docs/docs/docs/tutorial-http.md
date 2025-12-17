# Tutorial: Making HTTP Requests

This tutorial covers using Quadrate's `http` module to make HTTP and HTTPS requests. You'll learn how to fetch web pages, send data to APIs, and handle responses.

## Prerequisites

Make sure you have Quadrate installed and can run programs. The `http` module requires:

- The `net` module (TCP networking)
- The `tls` module (HTTPS support via OpenSSL)

## Simple GET Request

The easiest way to fetch a web page is with `http::get`:

```qd
use http

fn main( -- ) {
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

Key points:

- `http::get` is a **fallible function** - use `switch` to handle success and errors
- On success, you get a `Response` struct with `status`, `headers`, and `body` fields
- Use `@` to read struct fields: `resp @status`, `resp @body`
- Always call `http::close` to free resources

## Understanding the Response

The `http::Response` struct contains:

| Field | Type | Description |
|-------|------|-------------|
| `status` | `i64` | HTTP status code (200, 404, 500, etc.) |
| `headers` | `str` | Response headers as a string |
| `body` | `str` | Response body content |

```qd
use http

fn main( -- ) {
	"https://example.com" http::get switch {
		Ok {
			-> resp

			// Check status code using constants
			resp @status http::StatusOK eq
			if {
			    "Success!" print nl
			}

			// Print headers
			"Headers:" print nl
			resp @headers print nl

			// Print body
			"Body:" print nl
			resp @body print nl

			resp http::close
		}
		_ {
			"Request failed" print nl
		}
	}
}
```

## Using the Request Builder

For more control over your requests, use the builder pattern:

```qd
use http

fn main( -- ) {
	// Create a new request
	"https://example.com/api" http::new -> req

	// Set the HTTP method
	req "POST" http::method

	// Add headers
	req "Content-Type" "application/json" http::header
	req "Authorization" "Bearer my-token" http::header

	// Set the request body
	req "hello world" http::body

	// Send the request
	req http::send switch {
		Ok {
			-> resp
			"Status: " print resp @status print nl
			resp http::close
		}
		_ {
			"Request failed" print nl
		}
	}

	// Clean up the request object
	req http::free_request
}
```

### Builder Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `http::new` | `(url -- req)` | Create new request |
| `http::method` | `(req method --)` | Set HTTP method |
| `http::header` | `(req name value --)` | Add a header |
| `http::body` | `(req body --)` | Set request body |
| `http::send` | `(req -- resp)!` | Execute request |
| `http::free_request` | `(req --)` | Free request object |

## HTTP Methods

Common HTTP methods you can use:

```qd
req "GET" http::method     // Retrieve data
req "POST" http::method    // Submit data
req "PUT" http::method     // Update data
req "DELETE" http::method  // Delete data
req "PATCH" http::method   // Partial update
```

## HTTP Status Codes

The module provides constants for common HTTP status codes:

**1xx Informational:**

| Constant | Value | Description |
|----------|-------|-------------|
| `StatusContinue` | 100 | Continue |
| `StatusSwitchingProtocols` | 101 | Switching Protocols |
| `StatusProcessing` | 102 | Processing |
| `StatusEarlyHints` | 103 | Early Hints |

**2xx Success:**

| Constant | Value | Description |
|----------|-------|-------------|
| `StatusOK` | 200 | OK |
| `StatusCreated` | 201 | Created |
| `StatusAccepted` | 202 | Accepted |
| `StatusNonAuthoritativeInfo` | 203 | Non-Authoritative Information |
| `StatusNoContent` | 204 | No Content |
| `StatusResetContent` | 205 | Reset Content |
| `StatusPartialContent` | 206 | Partial Content |
| `StatusMultiStatus` | 207 | Multi-Status |
| `StatusAlreadyReported` | 208 | Already Reported |
| `StatusIMUsed` | 226 | IM Used |

**3xx Redirection:**

| Constant | Value | Description |
|----------|-------|-------------|
| `StatusMultipleChoices` | 300 | Multiple Choices |
| `StatusMovedPermanently` | 301 | Moved Permanently |
| `StatusFound` | 302 | Found |
| `StatusSeeOther` | 303 | See Other |
| `StatusNotModified` | 304 | Not Modified |
| `StatusUseProxy` | 305 | Use Proxy |
| `StatusTemporaryRedirect` | 307 | Temporary Redirect |
| `StatusPermanentRedirect` | 308 | Permanent Redirect |

**4xx Client Error:**

| Constant | Value | Description |
|----------|-------|-------------|
| `StatusBadRequest` | 400 | Bad Request |
| `StatusUnauthorized` | 401 | Unauthorized |
| `StatusPaymentRequired` | 402 | Payment Required |
| `StatusForbidden` | 403 | Forbidden |
| `StatusNotFound` | 404 | Not Found |
| `StatusMethodNotAllowed` | 405 | Method Not Allowed |
| `StatusNotAcceptable` | 406 | Not Acceptable |
| `StatusProxyAuthRequired` | 407 | Proxy Authentication Required |
| `StatusRequestTimeout` | 408 | Request Timeout |
| `StatusConflict` | 409 | Conflict |
| `StatusGone` | 410 | Gone |
| `StatusLengthRequired` | 411 | Length Required |
| `StatusPreconditionFailed` | 412 | Precondition Failed |
| `StatusPayloadTooLarge` | 413 | Payload Too Large |
| `StatusURITooLong` | 414 | URI Too Long |
| `StatusUnsupportedMediaType` | 415 | Unsupported Media Type |
| `StatusRangeNotSatisfiable` | 416 | Range Not Satisfiable |
| `StatusExpectationFailed` | 417 | Expectation Failed |
| `StatusTeapot` | 418 | I'm a teapot |
| `StatusMisdirectedRequest` | 421 | Misdirected Request |
| `StatusUnprocessableEntity` | 422 | Unprocessable Entity |
| `StatusLocked` | 423 | Locked |
| `StatusFailedDependency` | 424 | Failed Dependency |
| `StatusTooEarly` | 425 | Too Early |
| `StatusUpgradeRequired` | 426 | Upgrade Required |
| `StatusPreconditionRequired` | 428 | Precondition Required |
| `StatusTooManyRequests` | 429 | Too Many Requests |
| `StatusRequestHeaderFieldsTooLarge` | 431 | Request Header Fields Too Large |
| `StatusUnavailableForLegalReasons` | 451 | Unavailable For Legal Reasons |

**5xx Server Error:**

| Constant | Value | Description |
|----------|-------|-------------|
| `StatusInternalServerError` | 500 | Internal Server Error |
| `StatusNotImplemented` | 501 | Not Implemented |
| `StatusBadGateway` | 502 | Bad Gateway |
| `StatusServiceUnavailable` | 503 | Service Unavailable |
| `StatusGatewayTimeout` | 504 | Gateway Timeout |
| `StatusHTTPVersionNotSupported` | 505 | HTTP Version Not Supported |
| `StatusVariantAlsoNegotiates` | 506 | Variant Also Negotiates |
| `StatusInsufficientStorage` | 507 | Insufficient Storage |
| `StatusLoopDetected` | 508 | Loop Detected |
| `StatusNotExtended` | 510 | Not Extended |
| `StatusNetworkAuthRequired` | 511 | Network Authentication Required |

Use these constants instead of magic numbers:

```qd
use http

fn main( -- ) {
	"https://example.com" http::get switch {
		Ok {
			-> resp
			resp @status http::StatusOK eq
			if {
			    "Request succeeded" print nl
			}
			resp @status http::StatusNotFound eq
			if {
			    "Page not found" print nl
			}
			resp http::close
		}
		_ {
			"Request failed" print nl
		}
	}
}
```

## Error Handling

The `http` module provides specific error codes:

| Error | Value | Description |
|-------|-------|-------------|
| `ErrConnect` | 2 | Connection failed |
| `ErrTls` | 3 | TLS/SSL error |
| `ErrTimeout` | 4 | Request timeout |
| `ErrParse` | 5 | Response parse error |
| `ErrInvalidUrl` | 6 | Invalid URL format |
| `ErrRedirect` | 7 | Too many redirects |

Handle specific errors:

```qd
use http

fn main( -- ) {
	"invalid-url" http::get switch {
		Ok {
			-> resp
			resp http::close
		}
		6 {
			"Invalid URL!" print nl
		}
		3 {
			"TLS error - check certificates" print nl
		}
		_ {
			"Other error" print nl
		}
	}
}
```

## HTTPS vs HTTP

The module automatically handles HTTPS:

- URLs starting with `https://` use TLS encryption
- URLs starting with `http://` use plain TCP
- Certificate verification is enabled by default

```qd
// HTTPS (encrypted)
"https://secure.example.com" http::get

// HTTP (unencrypted)
"http://example.com" http::get
```

## Complete Example: API Client

Here's a more complete example that fetches data from an API:

```qd
use http
use str

fn fetch_page(url:str -- ) {
	url http::get switch {
		Ok {
			-> resp

			resp @status 200 eq
			if {
			    "Page fetched successfully" print nl
			    "Content length: " print
			    resp @body str::len print nl
			} else {
			    "HTTP error: " print resp @status print nl
			}

			resp http::close
		}
		_ {
			"Failed to fetch page" print nl
		}
	}
}

fn main( -- ) {
	"https://example.com" fetch_page
}
```

## Tips and Best Practices

1. **Always close responses**: Call `http::close` to free memory
2. **Free requests**: Call `http::free_request` when using the builder
3. **Check status codes**: Don't assume success - check `resp @status`
4. **Handle errors**: Use `switch` to handle different error cases
5. **Use HTTPS**: Prefer `https://` for security

## Next Steps

- Learn about the [TLS module](../stdlib/tls.md) for low-level secure connections
- Explore the [Net module](../stdlib/net.md) for raw TCP sockets
- Check the [Standard Library](../stdlib/index.md) for more modules
