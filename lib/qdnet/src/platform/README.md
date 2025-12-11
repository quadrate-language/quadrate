# Platform Abstraction

Socket operations abstracted for portability.

## Structure

```
platform/
├── net_platform.h     # Platform-agnostic API
└── posix/
    └── net_socket.c   # POSIX implementation
```

## API

- `net_platform_listen()` - Create server socket
- `net_platform_accept()` - Accept connection
- `net_platform_connect()` - Connect to server
- `net_platform_send/receive()` - Data transfer
- `net_platform_close()` - Close socket

## Porting

Create `platform/<os>/net_socket.c` implementing `net_platform.h`.
