#include <stdnetqd/net.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/net_platform.h"

// Stack signature: ( port:i -- socket:i )
// Creates a server socket, binds to the port, and listens
qd_exec_result usr_net_listen(qd_context* ctx) {
	qd_stack_element_t port_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &port_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_net_listen: stack underflow\n");
		abort();
	}

	if (port_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in usr_net_listen: port must be an integer\n");
		abort();
	}

	int port = (int)port_elem.value.i;

	// Create server socket using platform abstraction
	net_socket_t server_fd = net_platform_listen(port);
	if (server_fd == NET_SOCKET_INVALID) {
		fprintf(stderr, "Fatal error in usr_net_listen: failed to create and bind socket (port %d may be in use)\n", port);
		abort();
	}

	// Push socket file descriptor to stack
	qd_push_i(ctx, (int64_t)server_fd);
	return (qd_exec_result){0};
}

// Stack signature: ( server_socket:i -- client_socket:i )
// Accepts a client connection (blocking)
qd_exec_result usr_net_accept(qd_context* ctx) {
	qd_stack_element_t socket_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_net_accept: stack underflow\n");
		abort();
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in usr_net_accept: socket must be an integer\n");
		abort();
	}

	net_socket_t server_fd = (net_socket_t)socket_elem.value.i;

	// Accept connection using platform abstraction
	net_socket_t client_fd = net_platform_accept(server_fd);
	if (client_fd == NET_SOCKET_INVALID) {
		fprintf(stderr, "Fatal error in usr_net_accept: failed to accept connection\n");
		abort();
	}

	// Push client socket to stack
	qd_push_i(ctx, (int64_t)client_fd);
	return (qd_exec_result){0};
}

// Stack signature: ( host:s port:i -- socket:i )
// Connects to a remote host
qd_exec_result usr_net_connect(qd_context* ctx) {
	qd_stack_element_t port_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &port_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_net_connect: stack underflow\n");
		abort();
	}

	qd_stack_element_t host_elem;
	err = qd_stack_pop(ctx->st, &host_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_net_connect: stack underflow\n");
		abort();
	}

	if (port_elem.type != QD_STACK_TYPE_INT) {
		if (host_elem.type == QD_STACK_TYPE_STR) free(host_elem.value.s);
		fprintf(stderr, "Fatal error in usr_net_connect: port must be an integer\n");
		abort();
	}

	if (host_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in usr_net_connect: host must be a string\n");
		abort();
	}

	int port = (int)port_elem.value.i;
	char* host = host_elem.value.s;

	// Connect to remote host using platform abstraction
	net_socket_t sock_fd = net_platform_connect(host, port);
	free(host);

	if (sock_fd == NET_SOCKET_INVALID) {
		fprintf(stderr, "Fatal error in usr_net_connect: failed to connect to %s:%d\n", host, port);
		abort();
	}

	// Push socket to stack
	qd_push_i(ctx, (int64_t)sock_fd);
	return (qd_exec_result){0};
}

// Stack signature: ( socket:i data:s -- bytes_sent:i )
// Sends data to a socket
qd_exec_result usr_net_send(qd_context* ctx) {
	qd_stack_element_t data_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &data_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_net_send: stack underflow\n");
		abort();
	}

	qd_stack_element_t socket_elem;
	err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		if (data_elem.type == QD_STACK_TYPE_STR) free(data_elem.value.s);
		fprintf(stderr, "Fatal error in usr_net_send: stack underflow\n");
		abort();
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		if (data_elem.type == QD_STACK_TYPE_STR) free(data_elem.value.s);
		fprintf(stderr, "Fatal error in usr_net_send: socket must be an integer\n");
		abort();
	}

	if (data_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in usr_net_send: data must be a string\n");
		abort();
	}

	net_socket_t sock_fd = (net_socket_t)socket_elem.value.i;
	char* data = data_elem.value.s;
	size_t len = strlen(data);

	// Send data using platform abstraction
	int bytes_sent = net_platform_send(sock_fd, data, len);
	free(data);

	if (bytes_sent < 0) {
		fprintf(stderr, "Fatal error in usr_net_send: failed to send data\n");
		abort();
	}

	// Push bytes sent to stack
	qd_push_i(ctx, (int64_t)bytes_sent);
	return (qd_exec_result){0};
}

// Stack signature: ( socket:i max_bytes:i -- data:s bytes_read:i )
// Receives data from a socket
qd_exec_result usr_net_receive(qd_context* ctx) {
	qd_stack_element_t max_bytes_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &max_bytes_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_net_receive: stack underflow\n");
		abort();
	}

	qd_stack_element_t socket_elem;
	err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_net_receive: stack underflow\n");
		abort();
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in usr_net_receive: socket must be an integer\n");
		abort();
	}

	if (max_bytes_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in usr_net_receive: max_bytes must be an integer\n");
		abort();
	}

	net_socket_t sock_fd = (net_socket_t)socket_elem.value.i;
	int max_bytes = (int)max_bytes_elem.value.i;

	if (max_bytes <= 0 || max_bytes > 1048576) { // Max 1MB
		fprintf(stderr, "Fatal error in usr_net_receive: max_bytes must be between 1 and 1048576\n");
		abort();
	}

	// Allocate buffer
	char* buffer = malloc((size_t)max_bytes + 1);
	if (buffer == NULL) {
		fprintf(stderr, "Fatal error in usr_net_receive: failed to allocate buffer\n");
		abort();
	}

	// Read data using platform abstraction
	int bytes_read = net_platform_receive(sock_fd, buffer, (size_t)max_bytes);
	if (bytes_read < 0) {
		free(buffer);
		fprintf(stderr, "Fatal error in usr_net_receive: failed to read from socket\n");
		abort();
	}

	buffer[bytes_read] = '\0';

	// Push data string and bytes read to stack
	qd_push_s(ctx, buffer);
	qd_push_i(ctx, (int64_t)bytes_read);

	free(buffer);
	return (qd_exec_result){0};
}

// Stack signature: ( socket:i -- )
// Gracefully shuts down a socket for writing
qd_exec_result usr_net_shutdown(qd_context* ctx) {
	qd_stack_element_t socket_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_net_shutdown: stack underflow\n");
		abort();
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in usr_net_shutdown: socket must be an integer\n");
		abort();
	}

	net_socket_t sock_fd = (net_socket_t)socket_elem.value.i;
	net_platform_shutdown(sock_fd);

	return (qd_exec_result){0};
}

// Stack signature: ( socket:i -- )
// Closes a socket
qd_exec_result usr_net_close(qd_context* ctx) {
	qd_stack_element_t socket_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_net_close: stack underflow\n");
		abort();
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in usr_net_close: socket must be an integer\n");
		abort();
	}

	net_socket_t sock_fd = (net_socket_t)socket_elem.value.i;
	net_platform_close(sock_fd);

	return (qd_exec_result){0};
}
