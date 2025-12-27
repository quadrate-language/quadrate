# `use` net

TCP network operations.
Error codes: Ok=1 (success), specific errors start at 2

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `ErrAccept` | `3` | Error: Accept failed. |
| `ErrConnect` | `4` | Error: Connection failed. |
| `ErrInvalidArg` | `7` | Error: Invalid argument. |
| `ErrListen` | `2` | Error: Listen/bind failed. |
| `ErrReceive` | `6` | Error: Receive failed. |
| `ErrSend` | `5` | Error: Send failed. |

## Functions

### `fn` accept

Accept an incoming connection.

**Signature:** `(server_socket:i64 -- client_socket:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `server_socket` | `i64` | Server socket from listen |

| Output | Type | Description |
|--------|------|-------------|
| `client_socket` | `i64` | Connected client socket |

| Error | Description |
|-------|-------------|
| `net::ErrAccept` | Failed to accept connection |

**Example:**

```qd
server net::accept!  // client
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

### `fn` connect

Connect to a remote host.

**Signature:** `(host:str port:i64 -- socket:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `host` | `str` | Hostname or IP address |
| `port` | `i64` | Port number |

| Output | Type | Description |
|--------|------|-------------|
| `socket` | `i64` | Connected socket descriptor |

| Error | Description |
|-------|-------------|
| `net::ErrConnect` | Host unreachable or connection refused |

**Example:**

```qd
"localhost" 8080 net::connect!  // sock
```
---

### `fn` listen

Start listening for connections on a port.

**Signature:** `(port:i64 -- socket:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `port` | `i64` | Port number to listen on |

| Output | Type | Description |
|--------|------|-------------|
| `socket` | `i64` | Server socket descriptor |

| Error | Description |
|-------|-------------|
| `net::ErrListen` | Port may be in use or permission denied |

**Example:**

```qd
8080 net::listen!  // server
```
---

### `fn` receive

Receive data from socket.

**Signature:** `(socket:i64 max_bytes:i64 -- data:str bytes_read:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | Socket descriptor |
| `max_bytes` | `i64` | Maximum bytes to receive |

| Output | Type | Description |
|--------|------|-------------|
| `data` | `str` | Received data |
| `bytes_read` | `i64` | Actual bytes received |

| Error | Description |
|-------|-------------|
| `net::ErrReceive` | Failed to receive data |

**Example:**

```qd
sock 1024 net::receive! -> data  // n
```
---

### `fn` send

Send data over socket.

**Signature:** `(socket:i64 data:str -- bytes_sent:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | Socket descriptor |
| `data` | `str` | Data to send |

| Output | Type | Description |
|--------|------|-------------|
| `bytes_sent` | `i64` | Number of bytes sent |

| Error | Description |
|-------|-------------|
| `net::ErrSend` | Failed to send data |

**Example:**

```qd
sock "Hello" net::send!  // n
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
