# `use` tls

TLS/SSL secure socket operations using OpenSSL.

## Functions

### `fn` connect

Wrap a TCP socket with TLS encryption (client mode).

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
"example.com" 443 net::connect -> sock
sock "example.com" tls::connect! -> conn
```

---

### `fn` accept

Wrap a TCP socket with TLS encryption (server mode).

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
server net::accept -> client_sock
client_sock "/path/cert.pem" "/path/key.pem" tls::accept! -> conn
```

---

### `fn` send

Send data over TLS connection.

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
conn "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n" tls::send! -> n
```

---

### `fn` receive

Receive data from TLS connection.

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
conn 4096 tls::receive! -> data -> n
data print
```

---

### `fn` close

Close TLS connection and free resources.

**Signature:** `(conn:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `conn` | `ptr` | TLS connection handle |

**Note:** This does NOT close the underlying socket. Call `net::close` separately.

**Example:**

```qd
conn tls::close
sock net::close
```

---

## Error Codes

| Constant | Value | Description |
|----------|-------|-------------|
| `ErrInit` | 2 | TLS initialization failed |
| `ErrConnect` | 3 | TLS handshake failed (client mode) |
| `ErrAccept` | 4 | TLS handshake failed (server mode) |
| `ErrCertificate` | 5 | Certificate validation or loading error |
| `ErrRead` | 6 | TLS read operation failed |
| `ErrWrite` | 7 | TLS write operation failed |
| `ErrClosed` | 8 | Connection closed by peer |
| `ErrMemory` | 9 | Memory allocation failed |
| `ErrInvalidArg` | 10 | Invalid argument provided |

---

## Examples

### HTTPS Client (Low-Level)

```qd
use net
use tls

fn main() {
    // Connect to server
    "example.com" 443 net::connect -> sock

    // Wrap with TLS
    sock "example.com" tls::connect switch {
        Ok {
            -> conn

            // Send HTTP request
            conn "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n" tls::send! drop

            // Receive response
            conn 8192 tls::receive switch {
                Ok {
                    -> data drop
                    data print
                }
                _ {
                    "Receive failed" print nl
                }
            }

            // Cleanup
            conn tls::close
        }
        _ {
            "TLS handshake failed" print nl
        }
    }

    sock net::close
}
```

### TLS Server

```qd
use net
use tls

fn handle_client(client_sock:i64 -- ) {
    client_sock "/path/cert.pem" "/path/key.pem" tls::accept switch {
        Ok {
            -> conn

            // Receive request
            conn 4096 tls::receive switch {
                Ok {
                    -> data drop
                    "Received: " print data print nl

                    // Send response
                    conn "HTTP/1.1 200 OK\r\n\r\nHello, TLS!" tls::send! drop
                }
                _ { }
            }

            conn tls::close
        }
        tls::ErrCertificate {
            "Certificate error" print nl
        }
        _ {
            "TLS accept failed" print nl
        }
    }

    client_sock net::close
}

fn main() {
    443 net::listen -> server

    loop {
        server net::accept handle_client
    }
}
```

### Error Handling

```qd
use net
use tls

fn main() {
    "secure.example.com" 443 net::connect -> sock

    sock "secure.example.com" tls::connect switch {
        Ok {
            -> conn
            "Connected securely!" print nl
            conn tls::close
        }
        tls::ErrConnect {
            "TLS handshake failed" print nl
        }
        tls::ErrCertificate {
            "Certificate validation failed" print nl
        }
        _ {
            "Unknown TLS error" print nl
        }
    }

    sock net::close
}
```

---

## Notes

- The `http` module provides a higher-level API that handles TLS automatically
- Use `tls` directly when you need low-level control over secure connections
- Certificate verification is enabled by default
- Server mode requires valid certificate and key files in PEM format
