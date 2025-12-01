# Network Platform Abstraction Layer

This directory contains platform-specific socket implementations for the Quadrate network module.

## Structure

```
platform/
├── net_platform.h      # Platform-agnostic API (interface)
└── posix/
    └── net_socket.c    # POSIX socket implementation
```

## Design

The platform abstraction layer separates platform-specific networking code from the Quadrate runtime integration. This makes it easy to port Quadrate to different platforms by implementing the platform API.

### Platform API (net_platform.h)

The platform API defines a set of functions that must be implemented for each platform:

- `net_platform_init()` - Initialize networking subsystem
- `net_platform_cleanup()` - Cleanup networking subsystem
- `net_platform_listen()` - Create TCP server socket
- `net_platform_accept()` - Accept incoming connection
- `net_platform_connect()` - Connect to remote host
- `net_platform_send()` - Send data to socket
- `net_platform_receive()` - Receive data from socket
- `net_platform_shutdown()` - Graceful socket shutdown
- `net_platform_close()` - Close socket

### POSIX Implementation

The POSIX implementation (`posix/net_socket.c`) uses standard Berkeley sockets:
- `socket()`, `bind()`, `listen()`, `accept()`, `connect()`
- `read()`, `write()`, `shutdown()`, `close()`
- `gethostbyname()` for DNS resolution

This implementation works on Linux, BSD, macOS, and other POSIX-compliant systems.

## Adding a New Platform

To add support for a new platform (e.g., Windows with Winsock2):

1. Create a new directory: `platform/win32/`
2. Implement all functions from `net_platform.h` in `win32/net_socket.c`
3. Update `../meson.build` to conditionally compile the correct implementation:

```python
if host_machine.system() == 'windows'
    platform_impl = files('src/platform/win32/net_socket.c')
else
    platform_impl = files('src/platform/posix/net_socket.c')
endif

stdnetqd_sources = files('src/net.c') + platform_impl
```

### Example: Windows Implementation Notes

Windows uses Winsock2 instead of POSIX sockets. Key differences:

- Include `<winsock2.h>` and `<ws2tcpip.h>` instead of POSIX headers
- Call `WSAStartup()` in `net_platform_init()`
- Call `WSACleanup()` in `net_platform_cleanup()`
- Use `closesocket()` instead of `close()`
- Use `recv()`/`send()` instead of `read()`/`write()`
- Socket type is `SOCKET` (unsigned int) instead of `int`
- Link with `ws2_32.lib`

## Philosophy

Quadrate officially supports POSIX systems. Platform-specific implementations are provided as examples for those who wish to port Quadrate to other platforms, but non-POSIX systems are not officially supported.

The abstraction layer is designed to be minimal and focused on networking primitives, making it straightforward to implement on any platform with socket support.
