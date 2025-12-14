# net

TCP network operations.

## Functions

### `fn` accept

Accept an incoming connection.

**Signature:** `( server_socket:i64 -- client_socket:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `server_socket` | `i64` | Server socket from listen |

| Output | Type | Description |
|--------|------|-------------|
| `client_socket` | `i64` | Connected client socket |

**Example:**

```qd
server net::accept  // client
```

---

### `fn` close

Close socket and release resources.

**Signature:** `( socket:i64 -- )`

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

**Signature:** `( host:str port:i64 -- socket:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `host` | `str` | Hostname or IP address |
| `port` | `i64` | Port number |

| Output | Type | Description |
|--------|------|-------------|
| `socket` | `i64` | Connected socket descriptor |

**Example:**

```qd
"localhost" 8080 net::connect  // sock
```

---

### `fn` listen

Start listening for connections on a port.

**Signature:** `( port:i64 -- socket:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `port` | `i64` | Port number to listen on |

| Output | Type | Description |
|--------|------|-------------|
| `socket` | `i64` | Server socket descriptor |

**Example:**

```qd
8080 net::listen  // server
```

---

### `fn` receive

Receive data from socket.

**Signature:** `( socket:i64 max_bytes:i64 -- data:str bytes_read:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | Socket descriptor |
| `max_bytes` | `i64` | Maximum bytes to receive |

| Output | Type | Description |
|--------|------|-------------|
| `data` | `str` | Received data |
| `bytes_read` | `i64` | Actual bytes received |

**Example:**

```qd
sock 1024 net::receive -> data  // n
```

---

### `fn` send

Send data over socket.

**Signature:** `( socket:i64 data:str -- bytes_sent:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | Socket descriptor |
| `data` | `str` | Data to send |

| Output | Type | Description |
|--------|------|-------------|
| `bytes_sent` | `i64` | Number of bytes sent |

**Example:**

```qd
sock "Hello" net::send  // n
```

---

### `fn` shutdown

Shutdown socket for reading/writing.

**Signature:** `( socket:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | Socket descriptor |

**Example:**

```qd
sock net::shutdown
```
