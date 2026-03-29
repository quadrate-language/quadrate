# `use` tls

TLS/SSL secure socket operations.
Provides encryption layer on top of TCP sockets using OpenSSL.
Error codes: Ok=1 (success), specific errors start at 2

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `ErrAccept` | `4` | TLS handshake failed (server mode). |
| `ErrCertificate` | `5` | Certificate validation or loading error. |
| `ErrClosed` | `8` | Connection closed by peer. |
| `ErrConnect` | `3` | TLS handshake failed (client mode). |
| `ErrInit` | `2` | TLS initialization failed. |
| `ErrInvalidArg` | `10` | Invalid argument provided. |
| `ErrMemory` | `9` | Memory allocation failed. |
| `ErrRead` | `6` | TLS read operation failed. |
| `ErrWrite` | `7` | TLS write operation failed. |

## Functions

### `fn` accept

Wrap a TCP socket with TLS encryption (server mode). Loads certificate and key, then performs TLS handshake.

**Signature:** `(socket:i64 cert_path:str key_path:str -- conn:ptr)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | TCP socket from net::accept |
| `cert_path` | `str` | Path to PEM certificate file |
| `key_path` | `str` | Path to PEM private key file |

| Output | Type | Description |
|--------|------|-------------|
| `conn` | `ptr` | TLS connection handle |

**Example:**

```qd
client_sock "/path/cert.pem" "/path/key.pem" tls::accept!  // conn
```
---

### `fn` close

Close TLS connection and free resources. Performs TLS shutdown handshake. Does NOT close underlying socket. Call net::close separately to close the socket.

**Signature:** `(conn:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `conn` | `ptr` | TLS connection handle |

**Example:**

```qd
conn tls::close  sock net::close
```
---

### `fn` connect_mtls

Wrap a TCP socket with TLS encryption using client certificate (mTLS). Performs TLS handshake with client certificate authentication.

**Signature:** `(socket:i64 hostname:str cert_path:str key_path:str -- conn:ptr)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | TCP socket from net::connect |
| `hostname` | `str` | Server hostname for SNI and certificate verification |
| `cert_path` | `str` | Path to PEM client certificate file |
| `key_path` | `str` | Path to PEM client private key file |

| Output | Type | Description |
|--------|------|-------------|
| `conn` | `ptr` | TLS connection handle |

**Example:**

```qd
sock "api.example.com" "/path/client.crt" "/path/client.key" tls::connect_mtls!  // conn
```
---

### `fn` connect

Wrap a TCP socket with TLS encryption (client mode). Performs TLS handshake and certificate verification.

**Signature:** `(socket:i64 hostname:str -- conn:ptr)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `socket` | `i64` | TCP socket from net::connect |
| `hostname` | `str` | Server hostname for SNI and certificate verification |

| Output | Type | Description |
|--------|------|-------------|
| `conn` | `ptr` | TLS connection handle |

**Example:**

```qd
"example.com" 443 net::connect -> sock  sock "example.com" tls::connect!  // conn
```
---

### `fn` receive

Receive data from TLS connection. Data is decrypted after reception.

**Signature:** `(conn:ptr max_bytes:i64 -- data:str bytes_read:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `conn` | `ptr` | TLS connection handle |
| `max_bytes` | `i64` | Maximum bytes to receive |

| Output | Type | Description |
|--------|------|-------------|
| `data` | `str` | Received and decrypted data |
| `bytes_read` | `i64` | Actual bytes received |

**Example:**

```qd
conn 4096 tls::receive! -> data  // n
```
---

### `fn` send

Send data over TLS connection. Data is encrypted before transmission.

**Signature:** `(conn:ptr data:str -- bytes_sent:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `conn` | `ptr` | TLS connection handle |
| `data` | `str` | Data to send |

| Output | Type | Description |
|--------|------|-------------|
| `bytes_sent` | `i64` | Number of bytes sent |

**Example:**

```qd
conn "GET / HTTP/1.1\r\n\r\n" tls::send!  // n
```
