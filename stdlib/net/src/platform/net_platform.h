#ifndef QD_QDNET_NET_PLATFORM_H
#define QD_QDNET_NET_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Platform-agnostic socket type
typedef int net_socket_t;

// Socket error codes
#define NET_SOCKET_INVALID -1
#define NET_SUCCESS 0
#define NET_ERROR -1

// Initialize networking subsystem (required on some platforms like Windows)
// Returns 0 on success, non-zero on failure
int net_platform_init(void);

// Cleanup networking subsystem
void net_platform_cleanup(void);

// Create a TCP server socket, bind to port, and start listening
// Returns socket descriptor on success, NET_SOCKET_INVALID on failure
net_socket_t net_platform_listen(int port);

// Accept an incoming connection on a server socket
// Returns client socket descriptor on success, NET_SOCKET_INVALID on failure
net_socket_t net_platform_accept(net_socket_t server_socket);

// Connect to a remote host and port
// Returns socket descriptor on success, NET_SOCKET_INVALID on failure
net_socket_t net_platform_connect(const char* host, int port);

// Send data to a socket
// Returns number of bytes sent on success, -1 on failure
int net_platform_send(net_socket_t socket, const char* data, size_t len);

// Receive data from a socket
// Returns number of bytes received on success, -1 on failure, 0 on connection closed
int net_platform_receive(net_socket_t socket, char* buffer, size_t max_bytes);

// Shutdown a socket for writing (graceful shutdown)
// Returns 0 on success, -1 on failure
int net_platform_shutdown(net_socket_t socket);

// Close a socket
// Returns 0 on success, -1 on failure
int net_platform_close(net_socket_t socket);

// Set send/receive timeout on a socket (milliseconds)
// Returns 0 on success, -1 on failure
int net_platform_set_timeout(net_socket_t socket, int ms);

// Enable or disable TCP keepalive on a socket
// Returns 0 on success, -1 on failure
int net_platform_set_keepalive(net_socket_t socket, int enable);

// DNS lookup: resolve hostname to IP address string
// Writes IP address to ip_buf (must be at least 46 bytes for IPv6)
// Returns 0 on success, -1 on failure
int net_platform_lookup(const char* hostname, char* ip_buf, size_t ip_buf_len);

// Get peer address and port from connected socket
// Writes address string to addr_buf
// Returns 0 on success, -1 on failure
int net_platform_get_peer_addr(net_socket_t socket, char* addr_buf, size_t addr_buf_len, int* port);

#ifdef __cplusplus
}
#endif

#endif // QD_QDNET_NET_PLATFORM_H
