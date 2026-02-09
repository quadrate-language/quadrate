# `use` net

TCP network socket operations.

Provides functions for creating TCP servers, connecting to remote hosts, and
sending/receiving data over sockets. Sockets are represented as integer file
descriptors.

**Example:**

```qd
use net

fn main() {
    // Connect to a server
    "localhost" 8080 net::connect! -> sock

    // Send a message
    sock "Hello!\n" net::send! -> _

    // Receive up to 1024 bytes
    sock 1024 net::receive! -> n -> data
    data print nl

    // Clean up
    sock net::close
}
```

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `ErrListen` | `2` | Listen/bind failed. Port may be in use or permission denied. |
| `ErrAccept` | `3` | Accept failed. Could not accept incoming connection. |
| `ErrConnect` | `4` | Connection failed. Host unreachable or connection refused. |
| `ErrSend` | `5` | Send failed. Could not send data over socket. |
| `ErrReceive` | `6` | Receive failed. Could not receive data from socket. |
| `ErrInvalidArg` | `7` | Invalid argument passed to function. |

## Functions

### `fn` listen

Create a TCP server socket, bind to the specified port, and start listening
for connections. Sets `SO_REUSEADDR` and uses a backlog of 128.

**Signature:** `(port:i64 -- socket:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `port` | `i64` | Port number to listen on (0 for OS-assigned) |

| Output | Type | Description |
|--------|------|-------------|
| `socket` | `i64` | Server socket descriptor |

| Error | Description |
|-------|-------------|
| `net::ErrListen` | Port in use or permission denied |

**Example:**

```qd
8080 net::listen! -> server
```
---

### `fn` accept

Accept an incoming connection on a server socket. Blocks until a client
connects, then returns the client socket descriptor.

**Signature:** `(server_socket:i64 -- client_socket:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `server_socket` | `i64` | Server socket from `listen` |

| Output | Type | Description |
|--------|------|-------------|
| `client_socket` | `i64` | Connected client socket descriptor |

| Error | Description |
|-------|-------------|
| `net::ErrAccept` | Failed to accept connection |

**Example:**

```qd
server net::accept! -> client
```
---

### `fn` connect

Connect to a remote host and port. Resolves hostnames via DNS.

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
"localhost" 8080 net::connect! -> sock
```
---

### `fn` send

Send data over a connected socket.

**Signature:** `(socket:i64 data:str -- bytes_sent:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | Socket descriptor |
| `data` | `str` | Data to send |

| Output | Type | Description |
|--------|------|-------------|
| `bytes_sent` | `i64` | Number of bytes actually sent |

| Error | Description |
|-------|-------------|
| `net::ErrSend` | Failed to send data |

**Example:**

```qd
sock "Hello\n" net::send! -> n
```
---

### `fn` receive

Receive data from a connected socket. Reads up to `max_bytes` (maximum 1 MB).
Returns 0 bytes when the remote side has closed the connection.

**Signature:** `(socket:i64 max_bytes:i64 -- data:str bytes_read:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | Socket descriptor |
| `max_bytes` | `i64` | Maximum bytes to receive (1 to 1048576) |

| Output | Type | Description |
|--------|------|-------------|
| `data` | `str` | Received data as a string |
| `bytes_read` | `i64` | Actual number of bytes received |

| Error | Description |
|-------|-------------|
| `net::ErrReceive` | Failed to receive data |
| `net::ErrInvalidArg` | `max_bytes` out of range |

**Note:** `bytes_read` is on top of the stack, so pop it first:

```qd
sock 1024 net::receive! -> n -> data
```
---

### `fn` shutdown

Gracefully shut down a socket for writing (`SHUT_WR`). The remote side will
see end-of-stream on their next read. Use before `close` for clean TCP
teardown.

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

Close a socket and release associated resources. Always close sockets when
done to avoid file descriptor leaks.

**Signature:** `(socket:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | Socket descriptor |

**Example:**

```qd
sock net::close
```

## Error Handling

Failable functions (`listen`, `accept`, `connect`, `send`, `receive`) return
an error code that can be handled with `switch`:

```qd
"localhost" 8080 net::connect switch {
    Ok { -> sock
        // Connected successfully, use sock
        sock net::close
    }
    _ {
        "Connection failed" print nl
    }
}
```

Or panic on error with `!`:

```qd
"localhost" 8080 net::connect! -> sock
```

## Examples

### Echo server

Listens on port 8080, accepts one client, and echoes back everything it
receives until the client disconnects.

```qd
use net

fn main() {
    8080 net::listen! -> server
    "Listening on port 8080" print nl

    server net::accept! -> client
    "Client connected" print nl

    loop {
        client 1024 net::receive! -> n -> data
        n 0 == if { break }
        client data net::send! -> _
    }

    client net::close
    server net::close
}
```

### Simple client

Connects to a server, sends a message, then reads the response.

```qd
use net

fn main() {
    "localhost" 8080 net::connect! -> sock

    sock "Hello, server!\n" net::send! -> _
    sock net::shutdown

    sock 4096 net::receive! -> n -> data
    data print nl

    sock net::close
}
```
