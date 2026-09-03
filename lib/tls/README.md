# tls

TLS/SSL secure socket module for Quadrate using OpenSSL.

## Installation

`tls` ships with the Quadrate toolchain, so there is nothing to install:

```quadrate
use tls
```

**Prerequisites**: OpenSSL development libraries must be installed:
- Debian/Ubuntu: `apt install libssl-dev`
- Fedora/RHEL: `dnf install openssl-devel`
- Arch: `pacman -S openssl`
- macOS: `brew install openssl`

## Usage

```quadrate
use net
use tls

fn main() {
    // HTTPS client example
    "example.com" 443 net::connect! -> sock
    sock "example.com" tls::connect! -> conn

    conn "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n" tls::send! -> sent
    conn 4096 tls::receive! -> response -> n

    response print nl

    conn tls::close
    sock net::close
}
```

## Functions

### Client Operations

- `connect(socket:i64 hostname:str -- conn:ptr)!` - Wrap TCP socket with TLS (client mode)
- `connect_mtls(socket:i64 hostname:str cert:str key:str -- conn:ptr)!` - Connect with client certificate (mTLS)

### Server Operations

- `accept(socket:i64 cert:str key:str -- conn:ptr)!` - Wrap TCP socket with TLS (server mode)

### Data Transfer

- `send(conn:ptr data:str -- bytes:i64)!` - Send encrypted data
- `receive(conn:ptr max:i64 -- data:str bytes:i64)!` - Receive and decrypt data

### Connection Management

- `close(conn:ptr --)` - Close TLS connection (does NOT close underlying socket)

## Error Constants

- `ErrInit` (2) - TLS initialization failed
- `ErrConnect` (3) - TLS handshake failed (client)
- `ErrAccept` (4) - TLS handshake failed (server)
- `ErrCertificate` (5) - Certificate error
- `ErrRead` (6) - TLS read error
- `ErrWrite` (7) - TLS write error
- `ErrClosed` (8) - Connection closed by peer
- `ErrMemory` (9) - Memory allocation failed
- `ErrInvalidArg` (10) - Invalid argument

## Security

- Minimum TLS 1.2 enforced
- Certificate verification enabled by default
- Server Name Indication (SNI) supported
- Hostname verification enabled

## License

Apache-2.0

## Contributing

Contributions welcome! Please open an issue or pull request on [GitHub](https://github.com/quadrate-language/quadrate).
