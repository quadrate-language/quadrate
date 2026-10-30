#include "platform/net_platform.h"
#include <errno.h>
#include <inttypes.h>
#include <quadrate/net/net.h>
#include <quadrate/rt/runtime.h>
#include <stdlib.h>
#include <string.h>

// Error codes matching net.qd
#define NET_ERR_OK 1 // Success (matches builtin Ok)
#define NET_ERR_LISTEN 2
#define NET_ERR_ACCEPT 3
#define NET_ERR_CONNECT 4
#define NET_ERR_SEND 5
#define NET_ERR_RECEIVE 6
#define NET_ERR_INVALID_ARG 7
#define NET_ERR_TIMEOUT 8
#define NET_ERR_LOOKUP 9

#define NET_MAX_RECEIVE (1024 * 1024)

static int fail_errno(qd_context* ctx, int code, const char* op, int err) {
	// A socket timeout (net::set_timeout) shows as EAGAIN
	if (err == EAGAIN || err == EWOULDBLOCK) {
		code = NET_ERR_TIMEOUT;
	}
	return qd_fail(ctx, code, "%s: %s", op, strerror(err));
}

static int check_port(qd_context* ctx, const char* op, int64_t port) {
	if (port < 0 || port > 65535) {
		return qd_fail(ctx, NET_ERR_INVALID_ARG, "%s: port %" PRId64 " is not between 0 and 65535", op, port);
	}
	return 0;
}

static int push_ok(qd_context* ctx) {
	return qd_push_i(ctx, NET_ERR_OK);
}

// Stack signature: ( port:i64 -- socket:i64 )!
int usr_net_listen(qd_context* ctx) {
	int64_t port;
	qd_args(ctx, "net::listen", "i", &port);
	if (check_port(ctx, "net::listen", port) != 0) {
		return NET_ERR_INVALID_ARG;
	}
	const net_socket_t server_fd = net_platform_listen((int)port);
	if (server_fd == NET_SOCKET_INVALID) {
		return fail_errno(ctx, NET_ERR_LISTEN, "net::listen", errno);
	}
	qd_push_i(ctx, (int64_t)server_fd);
	return push_ok(ctx);
}

// Stack signature: ( server_socket:i64 -- client_socket:i64 )!
int usr_net_accept(qd_context* ctx) {
	int64_t server_fd;
	qd_args(ctx, "net::accept", "i", &server_fd);
	const net_socket_t client_fd = net_platform_accept((net_socket_t)server_fd);
	if (client_fd == NET_SOCKET_INVALID) {
		return fail_errno(ctx, NET_ERR_ACCEPT, "net::accept", errno);
	}
	qd_push_i(ctx, (int64_t)client_fd);
	return push_ok(ctx);
}

// Stack signature: ( host:str port:i64 -- socket:i64 )!
int usr_net_connect(qd_context* ctx) {
	qd_string_t* host;
	int64_t port;
	qd_args(ctx, "net::connect", "si", &host, &port);
	if (check_port(ctx, "net::connect", port) != 0) {
		qd_string_release(host);
		return NET_ERR_INVALID_ARG;
	}
	const net_socket_t fd = net_platform_connect(qd_string_data(host), (int)port);
	const int err = errno;
	if (fd == NET_SOCKET_INVALID) {
		const int code = qd_fail(
				ctx, NET_ERR_CONNECT, "net::connect: %s:%" PRId64 ": %s", qd_string_data(host), port, strerror(err));
		qd_string_release(host);
		return code;
	}
	qd_string_release(host);
	qd_push_i(ctx, (int64_t)fd);
	return push_ok(ctx);
}

// Stack signature: ( socket:i64 data:str -- bytes_sent:i64 )!
// Sends all of `data`, which may take several writes
int usr_net_send(qd_context* ctx) {
	int64_t fd;
	qd_string_t* data;
	qd_args(ctx, "net::send", "is", &fd, &data);
	const size_t len = qd_string_length(data);
	const int sent = net_platform_send((net_socket_t)fd, qd_string_data(data), len);
	const int err = errno;
	qd_string_release(data);
	if (sent != NET_SUCCESS) {
		return fail_errno(ctx, NET_ERR_SEND, "net::send", err);
	}
	qd_push_i(ctx, (int64_t)len);
	return push_ok(ctx);
}

// Stack signature: ( socket:i64 max_bytes:i64 -- data:str bytes_read:i64 )!
int usr_net_receive(qd_context* ctx) {
	int64_t fd;
	int64_t max_bytes;
	qd_args(ctx, "net::receive", "ii", &fd, &max_bytes);
	if (max_bytes <= 0 || max_bytes > NET_MAX_RECEIVE) {
		return qd_fail(ctx, NET_ERR_INVALID_ARG, "net::receive: max_bytes %" PRId64 " is not between 1 and %d",
				max_bytes, NET_MAX_RECEIVE);
	}
	char* buffer = malloc((size_t)max_bytes);
	if (!buffer) {
		qd_fatal_raise(ctx, "net::receive", "Out of memory");
	}
	const int bytes_read = net_platform_receive((net_socket_t)fd, buffer, (size_t)max_bytes);
	const int err = errno;
	if (bytes_read < 0) {
		free(buffer);
		return fail_errno(ctx, NET_ERR_RECEIVE, "net::receive", err);
	}
	// All of it: the data may hold a NUL byte, which ended the string here before
	qd_string_t* data = qd_string_create_with_length(buffer, (size_t)bytes_read);
	free(buffer);
	qd_push_s_ref(ctx, data);
	qd_string_release(data);
	qd_push_i(ctx, (int64_t)bytes_read);
	return push_ok(ctx);
}

// Stack signature: ( socket:i64 -- )
// Gracefully shuts down a socket for writing
int usr_net_shutdown(qd_context* ctx) {
	int64_t fd;
	qd_args(ctx, "net::shutdown", "i", &fd);
	net_platform_shutdown((net_socket_t)fd);
	return 0;
}

// Stack signature: ( socket:i64 -- )
int usr_net_close(qd_context* ctx) {
	int64_t fd;
	qd_args(ctx, "net::close", "i", &fd);
	net_platform_close((net_socket_t)fd);
	return 0;
}

// Stack signature: ( socket:i64 ms:i64 -- )!
int usr_net_set_timeout(qd_context* ctx) {
	int64_t fd;
	int64_t ms;
	qd_args(ctx, "net::set_timeout", "ii", &fd, &ms);
	if (ms < 0 || ms > INT32_MAX) {
		return qd_fail(ctx, NET_ERR_INVALID_ARG, "net::set_timeout: %" PRId64 " ms is out of range", ms);
	}
	if (net_platform_set_timeout((net_socket_t)fd, (int)ms) != NET_SUCCESS) {
		return qd_fail(ctx, NET_ERR_TIMEOUT, "net::set_timeout: %s", strerror(errno));
	}
	return push_ok(ctx);
}

// Stack signature: ( socket:i64 enable:i64 -- )!
int usr_net_set_keepalive(qd_context* ctx) {
	int64_t fd;
	int64_t enable;
	qd_args(ctx, "net::set_keepalive", "ii", &fd, &enable);
	if (net_platform_set_keepalive((net_socket_t)fd, enable != 0) != NET_SUCCESS) {
		return qd_fail(ctx, NET_ERR_INVALID_ARG, "net::set_keepalive: %s", strerror(errno));
	}
	return push_ok(ctx);
}

// Stack signature: ( hostname:str -- ip:str )!
int usr_net_lookup(qd_context* ctx) {
	qd_string_t* host;
	qd_args(ctx, "net::lookup", "s", &host);
	char ip_buf[64];
	const int result = net_platform_lookup(qd_string_data(host), ip_buf, sizeof(ip_buf));
	if (result != NET_SUCCESS) {
		const int code = qd_fail(ctx, NET_ERR_LOOKUP, "net::lookup: could not resolve %s", qd_string_data(host));
		qd_string_release(host);
		return code;
	}
	qd_string_release(host);
	qd_push_s(ctx, ip_buf);
	return push_ok(ctx);
}

// Stack signature: ( socket:i64 -- addr:str port:i64 )!
int usr_net_get_peer_addr(qd_context* ctx) {
	int64_t fd;
	qd_args(ctx, "net::get_peer_addr", "i", &fd);
	char addr_buf[64];
	int port = 0;
	if (net_platform_get_peer_addr((net_socket_t)fd, addr_buf, sizeof(addr_buf), &port) != NET_SUCCESS) {
		return qd_fail(ctx, NET_ERR_INVALID_ARG, "net::get_peer_addr: %s", strerror(errno));
	}
	qd_push_s(ctx, addr_buf);
	qd_push_i(ctx, (int64_t)port);
	return push_ok(ctx);
}
