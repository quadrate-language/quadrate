#define _DEFAULT_SOURCE
// POSIX socket implementation
#include "../net_platform.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

// Initialize networking (no-op on POSIX)
int net_platform_init(void) {
	return NET_SUCCESS;
}

// Cleanup networking (no-op on POSIX)
void net_platform_cleanup(void) {
	// Nothing to do on POSIX
}

// Create a TCP server socket, bind to port, and start listening
net_socket_t net_platform_listen(int port) {
	// Create socket
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		return NET_SOCKET_INVALID;
	}

	// Allow address reuse
	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		close(server_fd);
		return NET_SOCKET_INVALID;
	}

	// Bind to port
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons((uint16_t)port);

	if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		close(server_fd);
		return NET_SOCKET_INVALID;
	}

	// Listen with backlog of 128
	if (listen(server_fd, 128) < 0) {
		close(server_fd);
		return NET_SOCKET_INVALID;
	}

	return server_fd;
}

// Where a send cannot say MSG_NOSIGNAL, the socket says SO_NOSIGPIPE instead, so that
// writing to a peer that has gone fails the send rather than ending the process
static void no_sigpipe(int fd) {
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
	int on = 1;
	setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#else
	(void)fd;
#endif
}

// Accept an incoming connection on a server socket
net_socket_t net_platform_accept(net_socket_t server_socket) {
	int client_fd;
	do {
		client_fd = accept(server_socket, NULL, NULL);
	} while (client_fd < 0 && errno == EINTR);
	if (client_fd < 0) {
		return NET_SOCKET_INVALID;
	}
	no_sigpipe(client_fd);
	return client_fd;
}

// Connect to a remote host and port, trying each address the name resolves to, IPv6 too.
// On failure errno says why; a name that does not resolve is EHOSTUNREACH.
net_socket_t net_platform_connect(const char* host, int port) {
	char service[8];
	snprintf(service, sizeof(service), "%d", port);
	struct addrinfo hints = {0};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	struct addrinfo* found = NULL;
	if (getaddrinfo(host, service, &hints, &found) != 0) {
		errno = EHOSTUNREACH;
		return NET_SOCKET_INVALID;
	}
	int fd = -1;
	int err = ECONNREFUSED;
	for (struct addrinfo* a = found; a != NULL && fd < 0; a = a->ai_next) {
		fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
		if (fd >= 0 && connect(fd, a->ai_addr, a->ai_addrlen) != 0) {
			err = errno;
			close(fd);
			fd = -1;
		}
	}
	freeaddrinfo(found);
	if (fd < 0) {
		errno = err;
		return NET_SOCKET_INVALID;
	}
	no_sigpipe(fd);
	return fd;
}

// Send all of the data: one send may take only part of it
int net_platform_send(net_socket_t socket, const char* data, size_t len) {
#ifdef MSG_NOSIGNAL
	const int flags = MSG_NOSIGNAL;
#else
	const int flags = 0;
#endif
	size_t off = 0;
	while (off < len) {
		const ssize_t sent = send(socket, data + off, len - off, flags);
		if (sent < 0 && errno == EINTR) {
			continue;
		}
		if (sent <= 0) {
			return NET_ERROR;
		}
		off += (size_t)sent;
	}
	return NET_SUCCESS;
}

// Receive data from a socket
int net_platform_receive(net_socket_t socket, char* buffer, size_t max_bytes) {
	ssize_t bytes_read;
	do {
		bytes_read = read(socket, buffer, max_bytes);
	} while (bytes_read < 0 && errno == EINTR);
	return (int)bytes_read;
}

// Shutdown a socket for writing (graceful shutdown)
int net_platform_shutdown(net_socket_t socket) {
	return shutdown(socket, SHUT_WR);
}

// Close a socket
int net_platform_close(net_socket_t socket) {
	return close(socket);
}

// Set send/receive timeout
int net_platform_set_timeout(net_socket_t socket, int ms) {
	struct timeval tv;
	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		return NET_ERROR;
	}
	if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
		return NET_ERROR;
	}
	return NET_SUCCESS;
}

// Enable/disable TCP keepalive
int net_platform_set_keepalive(net_socket_t socket, int enable) {
	int val = enable ? 1 : 0;
	if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) < 0) {
		return NET_ERROR;
	}
	return NET_SUCCESS;
}

// DNS lookup
int net_platform_lookup(const char* hostname, char* ip_buf, size_t ip_buf_len) {
	struct addrinfo hints, *result;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int ret = getaddrinfo(hostname, NULL, &hints, &result);
	if (ret != 0) {
		return NET_ERROR;
	}

	// Use first result — copy via memcpy to avoid alignment warnings
	if (result->ai_family == AF_INET) {
		struct sockaddr_in addr;
		memcpy(&addr, result->ai_addr, sizeof(addr));
		inet_ntop(AF_INET, &addr.sin_addr, ip_buf, (socklen_t)ip_buf_len);
	} else if (result->ai_family == AF_INET6) {
		struct sockaddr_in6 addr;
		memcpy(&addr, result->ai_addr, sizeof(addr));
		inet_ntop(AF_INET6, &addr.sin6_addr, ip_buf, (socklen_t)ip_buf_len);
	} else {
		freeaddrinfo(result);
		return NET_ERROR;
	}

	freeaddrinfo(result);
	return NET_SUCCESS;
}

// Get peer address
int net_platform_get_peer_addr(net_socket_t socket, char* addr_buf, size_t addr_buf_len, int* port) {
	struct sockaddr_storage addr;
	socklen_t addr_len = sizeof(addr);

	if (getpeername(socket, (struct sockaddr*)&addr, &addr_len) < 0) {
		return NET_ERROR;
	}

	if (addr.ss_family == AF_INET) {
		struct sockaddr_in* sin = (struct sockaddr_in*)&addr;
		inet_ntop(AF_INET, &sin->sin_addr, addr_buf, (socklen_t)addr_buf_len);
		*port = ntohs(sin->sin_port);
	} else if (addr.ss_family == AF_INET6) {
		struct sockaddr_in6* sin6 = (struct sockaddr_in6*)&addr;
		inet_ntop(AF_INET6, &sin6->sin6_addr, addr_buf, (socklen_t)addr_buf_len);
		*port = ntohs(sin6->sin6_port);
	} else {
		return NET_ERROR;
	}

	return NET_SUCCESS;
}
