// POSIX socket implementation
#include "../net_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

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

// Accept an incoming connection on a server socket
net_socket_t net_platform_accept(net_socket_t server_socket) {
	int client_fd = accept(server_socket, NULL, NULL);
	if (client_fd < 0) {
		return NET_SOCKET_INVALID;
	}
	return client_fd;
}

// Connect to a remote host and port
net_socket_t net_platform_connect(const char* host, int port) {
	// Create socket
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		return NET_SOCKET_INVALID;
	}

	// Resolve hostname
	struct hostent* server = gethostbyname(host);
	if (server == NULL) {
		close(sock_fd);
		return NET_SOCKET_INVALID;
	}

	// Setup address
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	memcpy(&addr.sin_addr.s_addr, server->h_addr_list[0], (size_t)server->h_length);
	addr.sin_port = htons((uint16_t)port);

	// Connect
	if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		close(sock_fd);
		return NET_SOCKET_INVALID;
	}

	return sock_fd;
}

// Send data to a socket
int net_platform_send(net_socket_t socket, const char* data, size_t len) {
	ssize_t bytes_sent = write(socket, data, len);
	return (int)bytes_sent;
}

// Receive data from a socket
int net_platform_receive(net_socket_t socket, char* buffer, size_t max_bytes) {
	ssize_t bytes_read = read(socket, buffer, max_bytes);
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
