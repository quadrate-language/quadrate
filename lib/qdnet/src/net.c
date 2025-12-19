#include <qdnet/net.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/net_platform.h"

// Error codes matching module.qd
#define NET_ERR_OK 1  // Success (matches builtin Ok)
#define NET_ERR_LISTEN 2
#define NET_ERR_ACCEPT 3
#define NET_ERR_CONNECT 4
#define NET_ERR_SEND 5
#define NET_ERR_RECEIVE 6
#define NET_ERR_INVALID_ARG 7

// Stack signature: ( port:i -- socket:i )!
// Creates a server socket, binds to the port, and listens
qd_exec_result usr_net_listen(qd_context* ctx) {
	qd_stack_element_t port_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &port_elem);
	if (err != QD_STACK_OK) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	if (port_elem.type != QD_STACK_TYPE_INT) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	int port = (int)port_elem.value.i;

	// Create server socket using platform abstraction
	net_socket_t server_fd = net_platform_listen(port);
	if (server_fd == NET_SOCKET_INVALID) {
		qd_push_i(ctx, NET_ERR_LISTEN);
		return (qd_exec_result){NET_ERR_LISTEN};
	}

	// Push socket file descriptor to stack, then Ok
	qd_push_i(ctx, (int64_t)server_fd);
	qd_push_i(ctx, NET_ERR_OK);
	return (qd_exec_result){0};
}

// Stack signature: ( server_socket:i -- client_socket:i )!
// Accepts a client connection (blocking)
qd_exec_result usr_net_accept(qd_context* ctx) {
	qd_stack_element_t socket_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	net_socket_t server_fd = (net_socket_t)socket_elem.value.i;

	// Accept connection using platform abstraction
	net_socket_t client_fd = net_platform_accept(server_fd);
	if (client_fd == NET_SOCKET_INVALID) {
		qd_push_i(ctx, NET_ERR_ACCEPT);
		return (qd_exec_result){NET_ERR_ACCEPT};
	}

	// Push client socket to stack, then Ok
	qd_push_i(ctx, (int64_t)client_fd);
	qd_push_i(ctx, NET_ERR_OK);
	return (qd_exec_result){0};
}

// Stack signature: ( host:s port:i -- socket:i )!
// Connects to a remote host
qd_exec_result usr_net_connect(qd_context* ctx) {
	qd_stack_element_t port_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &port_elem);
	if (err != QD_STACK_OK) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	qd_stack_element_t host_elem;
	err = qd_stack_pop(ctx->st, &host_elem);
	if (err != QD_STACK_OK) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	if (port_elem.type != QD_STACK_TYPE_INT) {
		if (host_elem.type == QD_STACK_TYPE_STR) qd_string_release(host_elem.value.s);
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	if (host_elem.type != QD_STACK_TYPE_STR) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	int port = (int)port_elem.value.i;
	const char* host = qd_string_data(host_elem.value.s);

	// Connect to remote host using platform abstraction
	net_socket_t sock_fd = net_platform_connect(host, port);

	if (sock_fd == NET_SOCKET_INVALID) {
		qd_string_release(host_elem.value.s);
		qd_push_i(ctx, NET_ERR_CONNECT);
		return (qd_exec_result){NET_ERR_CONNECT};
	}

	qd_string_release(host_elem.value.s);

	// Push socket to stack, then Ok
	qd_push_i(ctx, (int64_t)sock_fd);
	qd_push_i(ctx, NET_ERR_OK);
	return (qd_exec_result){0};
}

// Stack signature: ( socket:i data:s -- bytes_sent:i )!
// Sends data to a socket
qd_exec_result usr_net_send(qd_context* ctx) {
	qd_stack_element_t data_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &data_elem);
	if (err != QD_STACK_OK) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	qd_stack_element_t socket_elem;
	err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		if (data_elem.type == QD_STACK_TYPE_STR) qd_string_release(data_elem.value.s);
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		if (data_elem.type == QD_STACK_TYPE_STR) qd_string_release(data_elem.value.s);
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	if (data_elem.type != QD_STACK_TYPE_STR) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	net_socket_t sock_fd = (net_socket_t)socket_elem.value.i;
	const char* data = qd_string_data(data_elem.value.s);
	size_t len = strlen(data);

	// Send data using platform abstraction
	int bytes_sent = net_platform_send(sock_fd, data, len);
	qd_string_release(data_elem.value.s);

	if (bytes_sent < 0) {
		qd_push_i(ctx, NET_ERR_SEND);
		return (qd_exec_result){NET_ERR_SEND};
	}

	// Push bytes sent to stack, then Ok
	qd_push_i(ctx, (int64_t)bytes_sent);
	qd_push_i(ctx, NET_ERR_OK);
	return (qd_exec_result){0};
}

// Stack signature: ( socket:i max_bytes:i -- data:s bytes_read:i )!
// Receives data from a socket
qd_exec_result usr_net_receive(qd_context* ctx) {
	qd_stack_element_t max_bytes_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &max_bytes_elem);
	if (err != QD_STACK_OK) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	qd_stack_element_t socket_elem;
	err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	if (max_bytes_elem.type != QD_STACK_TYPE_INT) {
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	net_socket_t sock_fd = (net_socket_t)socket_elem.value.i;
	int max_bytes = (int)max_bytes_elem.value.i;

	if (max_bytes <= 0 || max_bytes > 1048576) { // Max 1MB
		qd_push_i(ctx, NET_ERR_INVALID_ARG);
		return (qd_exec_result){NET_ERR_INVALID_ARG};
	}

	// Allocate buffer
	char* buffer = malloc((size_t)max_bytes + 1);
	if (buffer == NULL) {
		qd_push_i(ctx, NET_ERR_RECEIVE);
		return (qd_exec_result){NET_ERR_RECEIVE};
	}

	// Read data using platform abstraction
	int bytes_read = net_platform_receive(sock_fd, buffer, (size_t)max_bytes);
	if (bytes_read < 0) {
		free(buffer);
		qd_push_i(ctx, NET_ERR_RECEIVE);
		return (qd_exec_result){NET_ERR_RECEIVE};
	}

	buffer[bytes_read] = '\0';

	// Push data string and bytes read to stack, then Ok
	qd_push_s(ctx, buffer);
	qd_push_i(ctx, (int64_t)bytes_read);
	qd_push_i(ctx, NET_ERR_OK);

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
