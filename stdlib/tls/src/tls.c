#define _DEFAULT_SOURCE
/**
 * @file tls.c
 * @brief TLS module implementation for Quadrate
 *
 * Bridges Quadrate runtime to TLS platform abstraction.
 */

#define _POSIX_C_SOURCE 200809L

#include "platform/tls_platform.h"
#include <inttypes.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/tls/tls.h>
#include <stdlib.h>

#define TLS_MAX_RECEIVE (1024 * 1024)

// The platform's codes are the module's: TLS_ERR_CONNECT is tls::ErrConnect, and so on
static int fail(qd_context* ctx, const char* op, tls_error_t err) {
	const char* detail = tls_platform_error_detail();
	if (detail[0] != '\0') {
		return qd_fail(ctx, (int)err, "%s: %s: %s", op, tls_platform_error_string(err), detail);
	}
	return qd_fail(ctx, (int)err, "%s: %s", op, tls_platform_error_string(err));
}

static int push_conn(qd_context* ctx, const char* op, tls_error_t err, tls_conn_t conn) {
	if (err != TLS_OK) {
		return fail(ctx, op, err);
	}
	qd_push_p(ctx, conn);
	return qd_push_i(ctx, TLS_OK);
}

// Stack signature: ( socket:i64 hostname:str -- conn:ptr )!
int usr_tls_connect(qd_context* ctx) {
	int64_t fd;
	qd_string_t* hostname;
	qd_args(ctx, "tls::connect", "is", &fd, &hostname);
	tls_conn_t conn = TLS_CONN_INVALID;
	const tls_error_t err = tls_platform_connect((int)fd, qd_string_data(hostname), &conn);
	qd_string_release(hostname);
	return push_conn(ctx, "tls::connect", err, conn);
}

// Stack signature: ( socket:i64 hostname:str cert_path:str key_path:str -- conn:ptr )!
int usr_tls_connect_mtls(qd_context* ctx) {
	int64_t fd;
	qd_string_t* hostname;
	qd_string_t* cert;
	qd_string_t* key;
	qd_args(ctx, "tls::connect_mtls", "isss", &fd, &hostname, &cert, &key);
	tls_conn_t conn = TLS_CONN_INVALID;
	const tls_error_t err = tls_platform_connect_mtls(
			(int)fd, qd_string_data(hostname), qd_string_data(cert), qd_string_data(key), &conn);
	qd_string_release(hostname);
	qd_string_release(cert);
	qd_string_release(key);
	return push_conn(ctx, "tls::connect_mtls", err, conn);
}

// Stack signature: ( socket:i64 cert_path:str key_path:str -- conn:ptr )!
int usr_tls_accept(qd_context* ctx) {
	int64_t fd;
	qd_string_t* cert;
	qd_string_t* key;
	qd_args(ctx, "tls::accept", "iss", &fd, &cert, &key);
	tls_conn_t conn = TLS_CONN_INVALID;
	const tls_error_t err = tls_platform_accept((int)fd, qd_string_data(cert), qd_string_data(key), &conn);
	qd_string_release(cert);
	qd_string_release(key);
	return push_conn(ctx, "tls::accept", err, conn);
}

// Stack signature: ( conn:ptr data:str -- bytes_sent:i64 )!
// Sends all of `data`
int usr_tls_send(qd_context* ctx) {
	void* conn;
	qd_string_t* data;
	qd_args(ctx, "tls::send", "ps", &conn, &data);
	const char* bytes = qd_string_data(data);
	const size_t len = qd_string_length(data);
	size_t off = 0;
	tls_error_t err = TLS_OK;
	while (off < len && err == TLS_OK) {
		int sent = 0;
		err = tls_platform_send(conn, bytes + off, len - off, &sent);
		off += err == TLS_OK ? (size_t)sent : 0;
	}
	qd_string_release(data);
	if (err != TLS_OK) {
		return fail(ctx, "tls::send", err);
	}
	qd_push_i(ctx, (int64_t)len);
	return qd_push_i(ctx, TLS_OK);
}

// Stack signature: ( conn:ptr max_bytes:i64 -- data:str bytes_read:i64 )!
// The peer closing the session cleanly is the end of the data, as it is for net::receive:
// "" and 0, not ErrClosed.
int usr_tls_receive(qd_context* ctx) {
	void* conn;
	int64_t max_bytes;
	qd_args(ctx, "tls::receive", "pi", &conn, &max_bytes);
	if (max_bytes <= 0 || max_bytes > TLS_MAX_RECEIVE) {
		return qd_fail(ctx, TLS_ERR_INVALID_ARG, "tls::receive: max_bytes %" PRId64 " is not between 1 and %d",
				max_bytes, TLS_MAX_RECEIVE);
	}
	char* buffer = malloc((size_t)max_bytes);
	if (!buffer) {
		qd_fatal_raise(ctx, "tls::receive", "Out of memory");
	}
	int bytes_read = 0;
	const tls_error_t err = tls_platform_receive(conn, buffer, (size_t)max_bytes, &bytes_read);
	if (err != TLS_OK && err != TLS_ERR_CLOSED) {
		free(buffer);
		return fail(ctx, "tls::receive", err);
	}
	// All of it: the data may hold a NUL byte, which ended the string here before
	qd_string_t* data = qd_string_create_with_length(buffer, err == TLS_OK ? (size_t)bytes_read : 0);
	free(buffer);
	qd_push_s_ref(ctx, data);
	qd_string_release(data);
	qd_push_i(ctx, err == TLS_OK ? (int64_t)bytes_read : 0);
	return qd_push_i(ctx, TLS_OK);
}

// Stack signature: ( conn:ptr -- )
int usr_tls_close(qd_context* ctx) {
	void* conn;
	qd_args(ctx, "tls::close", "p", &conn);
	if (conn != TLS_CONN_INVALID) {
		tls_platform_close(conn);
	}
	return 0;
}
