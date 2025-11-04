#include <stdqd/net.h>
#include <quadrate/runtime/runtime.h>
#include <quadrate/runtime/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

// Stack signature: ( port:i -- socket:i )
// Creates a server socket, binds to the port, and listens
qd_exec_result qd_stdqd_listen(qd_context* ctx) {
	qd_stack_element_t port_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &port_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in qd_stdqd_listen: stack underflow\n");
		abort();
	}

	if (port_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in qd_stdqd_listen: port must be an integer\n");
		abort();
	}

	int port = (int)port_elem.value.i;

	// Create socket
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		fprintf(stderr, "Fatal error in qd_stdqd_listen: failed to create socket\n");
		abort();
	}

	// Allow address reuse
	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		close(server_fd);
		fprintf(stderr, "Fatal error in qd_stdqd_listen: failed to set socket options\n");
		abort();
	}

	// Bind to port
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons((uint16_t)port);

	if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		close(server_fd);
		fprintf(stderr, "Fatal error in qd_stdqd_listen: failed to bind socket (port %d may be in use)\n", port);
		abort();
	}

	// Listen with backlog of 128
	if (listen(server_fd, 128) < 0) {
		close(server_fd);
		fprintf(stderr, "Fatal error in qd_stdqd_listen: failed to listen on socket\n");
		abort();
	}

	// Push socket file descriptor to stack
	qd_push_i(ctx, (int64_t)server_fd);
	return (qd_exec_result){0};
}

// Stack signature: ( server_socket:i -- client_socket:i )
// Accepts a client connection (blocking)
qd_exec_result qd_stdqd_accept(qd_context* ctx) {
	qd_stack_element_t socket_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in qd_stdqd_accept: stack underflow\n");
		abort();
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in qd_stdqd_accept: socket must be an integer\n");
		abort();
	}

	int server_fd = (int)socket_elem.value.i;

	// Accept connection
	int client_fd = accept(server_fd, NULL, NULL);
	if (client_fd < 0) {
		fprintf(stderr, "Fatal error in qd_stdqd_accept: failed to accept connection\n");
		abort();
	}

	// Push client socket to stack
	qd_push_i(ctx, (int64_t)client_fd);
	return (qd_exec_result){0};
}

// Stack signature: ( host:s port:i -- socket:i )
// Connects to a remote host
qd_exec_result qd_stdqd_connect(qd_context* ctx) {
	qd_stack_element_t port_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &port_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in qd_stdqd_connect: stack underflow\n");
		abort();
	}

	qd_stack_element_t host_elem;
	err = qd_stack_pop(ctx->st, &host_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in qd_stdqd_connect: stack underflow\n");
		abort();
	}

	if (port_elem.type != QD_STACK_TYPE_INT) {
		if (host_elem.type == QD_STACK_TYPE_STR) free(host_elem.value.s);
		fprintf(stderr, "Fatal error in qd_stdqd_connect: port must be an integer\n");
		abort();
	}

	if (host_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in qd_stdqd_connect: host must be a string\n");
		abort();
	}

	int port = (int)port_elem.value.i;
	char* host = host_elem.value.s;

	// Create socket
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		free(host);
		fprintf(stderr, "Fatal error in qd_stdqd_connect: failed to create socket\n");
		abort();
	}

	// Resolve hostname
	struct hostent* server = gethostbyname(host);
	if (server == NULL) {
		close(sock_fd);
		free(host);
		fprintf(stderr, "Fatal error in qd_stdqd_connect: failed to resolve hostname\n");
		abort();
	}

	// Setup address
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	memcpy(&addr.sin_addr.s_addr, server->h_addr_list[0], (size_t)server->h_length);
	addr.sin_port = htons((uint16_t)port);

	// Connect
	if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		close(sock_fd);
		free(host);
		fprintf(stderr, "Fatal error in qd_stdqd_connect: failed to connect\n");
		abort();
	}

	free(host);

	// Push socket to stack
	qd_push_i(ctx, (int64_t)sock_fd);
	return (qd_exec_result){0};
}

// Stack signature: ( socket:i data:s -- bytes_sent:i )
// Sends data to a socket
qd_exec_result qd_stdqd_send(qd_context* ctx) {
	qd_stack_element_t data_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &data_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in qd_stdqd_send: stack underflow\n");
		abort();
	}

	qd_stack_element_t socket_elem;
	err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		if (data_elem.type == QD_STACK_TYPE_STR) free(data_elem.value.s);
		fprintf(stderr, "Fatal error in qd_stdqd_send: stack underflow\n");
		abort();
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		if (data_elem.type == QD_STACK_TYPE_STR) free(data_elem.value.s);
		fprintf(stderr, "Fatal error in qd_stdqd_send: socket must be an integer\n");
		abort();
	}

	if (data_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in qd_stdqd_send: data must be a string\n");
		abort();
	}

	int sock_fd = (int)socket_elem.value.i;
	char* data = data_elem.value.s;
	size_t len = strlen(data);

	// Send data
	ssize_t bytes_sent = write(sock_fd, data, len);
	free(data);

	if (bytes_sent < 0) {
		fprintf(stderr, "Fatal error in qd_stdqd_send: failed to send data\n");
		abort();
	}

	// Push bytes sent to stack
	qd_push_i(ctx, (int64_t)bytes_sent);
	return (qd_exec_result){0};
}

// Stack signature: ( socket:i max_bytes:i -- data:s bytes_read:i )
// Receives data from a socket
qd_exec_result qd_stdqd_receive(qd_context* ctx) {
	qd_stack_element_t max_bytes_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &max_bytes_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in qd_stdqd_receive: stack underflow\n");
		abort();
	}

	qd_stack_element_t socket_elem;
	err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in qd_stdqd_receive: stack underflow\n");
		abort();
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in qd_stdqd_receive: socket must be an integer\n");
		abort();
	}

	if (max_bytes_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in qd_stdqd_receive: max_bytes must be an integer\n");
		abort();
	}

	int sock_fd = (int)socket_elem.value.i;
	int max_bytes = (int)max_bytes_elem.value.i;

	if (max_bytes <= 0 || max_bytes > 1048576) { // Max 1MB
		fprintf(stderr, "Fatal error in qd_stdqd_receive: max_bytes must be between 1 and 1048576\n");
		abort();
	}

	// Allocate buffer
	char* buffer = malloc((size_t)max_bytes + 1);
	if (buffer == NULL) {
		fprintf(stderr, "Fatal error in qd_stdqd_receive: failed to allocate buffer\n");
		abort();
	}

	// Read data
	ssize_t bytes_read = read(sock_fd, buffer, (size_t)max_bytes);
	if (bytes_read < 0) {
		free(buffer);
		fprintf(stderr, "Fatal error in qd_stdqd_receive: failed to read from socket\n");
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
// Gracefully shuts down a socket for writing (SHUT_WR)
qd_exec_result qd_stdqd_shutdown(qd_context* ctx) {
	qd_stack_element_t socket_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in qd_stdqd_shutdown: stack underflow\n");
		abort();
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in qd_stdqd_shutdown: socket must be an integer\n");
		abort();
	}

	int sock_fd = (int)socket_elem.value.i;
	shutdown(sock_fd, SHUT_WR);

	return (qd_exec_result){0};
}

// Stack signature: ( socket:i -- )
// Closes a socket
qd_exec_result qd_stdqd_close(qd_context* ctx) {
	qd_stack_element_t socket_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in qd_stdqd_close: stack underflow\n");
		abort();
	}

	if (socket_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in qd_stdqd_close: socket must be an integer\n");
		abort();
	}

	int sock_fd = (int)socket_elem.value.i;
	close(sock_fd);

	return (qd_exec_result){0};
}

