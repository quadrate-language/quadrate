# `use` net

TCP network operations.

## Error Codes

| Constant | Value | Description |
|----------|-------|-------------|
| `ErrListen` | 2 | Listen/bind failed (port in use or permission denied) |
| `ErrAccept` | 3 | Accept failed |
| `ErrConnect` | 4 | Connection failed (host unreachable or refused) |
| `ErrSend` | 5 | Send failed |
| `ErrReceive` | 6 | Receive failed |
| `ErrInvalidArg` | 7 | Invalid argument |

## Functions

### `fn` listen!

Start listening for connections on a port.

**Signature:** `(port:i64 -- socket:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `port` | `i64` | Port number to listen on |

| Output | Type | Description |
|--------|------|-------------|
| `socket` | `i64` | Server socket descriptor |

**Errors:** `ErrListen` - Port may be in use or permission denied

**Example:**

```qd
8080 net::listen switch {
	Ok {
		-> server
		"Listening on port 8080" print nl
	}
	net::ErrListen {
		"Failed to bind port" print nl
	}
}
```

---

### `fn` accept!

Accept an incoming connection.

**Signature:** `(server_socket:i64 -- client_socket:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `server_socket` | `i64` | Server socket from listen |

| Output | Type | Description |
|--------|------|-------------|
| `client_socket` | `i64` | Connected client socket |

**Errors:** `ErrAccept` - Failed to accept connection

**Example:**

```qd
server net::accept switch {
	Ok {
		-> client
		// Handle client connection
	}
	_ {
		"Accept failed" print nl
	}
}
```

---

### `fn` connect!

Connect to a remote host.

**Signature:** `(host:str port:i64 -- socket:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `host` | `str` | Hostname or IP address |
| `port` | `i64` | Port number |

| Output | Type | Description |
|--------|------|-------------|
| `socket` | `i64` | Connected socket descriptor |

**Errors:** `ErrConnect` - Host unreachable or connection refused

**Example:**

```qd
"localhost" 8080 net::connect switch {
	Ok {
		-> sock
		sock "Hello\n" net::send! drop
		sock net::close
	}
	net::ErrConnect {
		"Connection refused" print nl
	}
}
```

---

### `fn` send!

Send data over socket.

**Signature:** `(socket:i64 data:str -- bytes_sent:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | Socket descriptor |
| `data` | `str` | Data to send |

| Output | Type | Description |
|--------|------|-------------|
| `bytes_sent` | `i64` | Number of bytes sent |

**Errors:** `ErrSend` - Failed to send data

**Example:**

```qd
sock "Hello" net::send switch {
	Ok {
		-> n
		"Sent " print n print " bytes" print nl
	}
	_ {
		"Send failed" print nl
	}
}
```

---

### `fn` receive!

Receive data from socket.

**Signature:** `(socket:i64 max_bytes:i64 -- data:str bytes_read:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | Socket descriptor |
| `max_bytes` | `i64` | Maximum bytes to receive (1-1048576) |

| Output | Type | Description |
|--------|------|-------------|
| `data` | `str` | Received data |
| `bytes_read` | `i64` | Actual bytes received |

**Errors:** `ErrReceive` - Failed to receive data

**Example:**

```qd
sock 1024 net::receive switch {
	Ok {
		-> n -> data
		"Received: " print data print nl
	}
	_ {
		"Receive failed" print nl
	}
}
```

---

### `fn` shutdown

Shutdown socket for reading/writing.

**Signature:** `(socket:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | Socket descriptor |

**Example:**

```qd
sock net::shutdown
```

---

### `fn` close

Close socket and release resources.

**Signature:** `(socket:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | Socket descriptor |

**Example:**

```qd
sock net::close
```

---

## Complete Example: TCP Client

```qd
use net

fn main( -- ) {
	// Connect to a server
	"localhost" 8080 net::connect switch {
		Ok {
			-> sock

			// Send a request
			sock "GET / HTTP/1.0\r\n\r\n" net::send! drop

			// Receive response
			sock 4096 net::receive switch {
				Ok {
					-> n -> data
					data print
				}
				_ {
					"Receive failed" print nl
				}
			}

			// Clean up
			sock net::close
		}
		net::ErrConnect {
			"Connection failed" print nl
		}
	}
}
```
