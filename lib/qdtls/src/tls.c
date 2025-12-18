/**
 * @file tls.c
 * @brief TLS module implementation for Quadrate
 *
 * Bridges Quadrate runtime to TLS platform abstraction.
 */

#include <qdtls/tls.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/tls_platform.h"

// Stack signature: ( socket:i hostname:s -- tls_conn:ptr )
// Fallible function - pushes error code
qd_exec_result usr_tls_connect(qd_context* ctx) {
	// Pop hostname
	qd_stack_element_t hostname_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &hostname_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in tls::connect: stack underflow\n");
		abort();
	}
	
	// Pop socket
	qd_stack_element_t socket_elem;
	err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		if (hostname_elem.type == QD_STACK_TYPE_STR) qd_string_release(hostname_elem.value.s);
		fprintf(stderr, "Fatal error in tls::connect: stack underflow\n");
		abort();
	}
	
	// Type checks
	if (socket_elem.type != QD_STACK_TYPE_INT) {
		if (hostname_elem.type == QD_STACK_TYPE_STR) qd_string_release(hostname_elem.value.s);
		fprintf(stderr, "Fatal error in tls::connect: socket must be an integer\n");
		abort();
	}
	
	if (hostname_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in tls::connect: hostname must be a string\n");
		abort();
	}
	
	int socket_fd = (int)socket_elem.value.i;
	const char* hostname = qd_string_data(hostname_elem.value.s);
	
	// Perform TLS connect
	tls_conn_t conn = TLS_CONN_INVALID;
	tls_error_t tls_err = tls_platform_connect(socket_fd, hostname, &conn);

	qd_string_release(hostname_elem.value.s);

	if (tls_err != TLS_OK) {
		// On failure: set error and push only error code
		ctx->error_code = (int)tls_err;
		if (ctx->error_msg) free(ctx->error_msg);
		ctx->error_msg = strdup(tls_platform_error_string(tls_err));
		qd_push_i(ctx, (int64_t)tls_err);
		return (qd_exec_result){(int)tls_err};
	}

	// On success: push result, then Ok
	qd_push_p(ctx, conn);
	qd_push_i(ctx, TLS_OK);
	return (qd_exec_result){0};
}

// Stack signature: ( socket:i hostname:s cert_path:s key_path:s -- tls_conn:ptr )
// Fallible function - pushes error code
qd_exec_result usr_tls_connect_mtls(qd_context* ctx) {
	// Pop key_path
	qd_stack_element_t key_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &key_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in tls::connect_mtls: stack underflow\n");
		abort();
	}

	// Pop cert_path
	qd_stack_element_t cert_elem;
	err = qd_stack_pop(ctx->st, &cert_elem);
	if (err != QD_STACK_OK) {
		if (key_elem.type == QD_STACK_TYPE_STR) qd_string_release(key_elem.value.s);
		fprintf(stderr, "Fatal error in tls::connect_mtls: stack underflow\n");
		abort();
	}

	// Pop hostname
	qd_stack_element_t hostname_elem;
	err = qd_stack_pop(ctx->st, &hostname_elem);
	if (err != QD_STACK_OK) {
		if (key_elem.type == QD_STACK_TYPE_STR) qd_string_release(key_elem.value.s);
		if (cert_elem.type == QD_STACK_TYPE_STR) qd_string_release(cert_elem.value.s);
		fprintf(stderr, "Fatal error in tls::connect_mtls: stack underflow\n");
		abort();
	}

	// Pop socket
	qd_stack_element_t socket_elem;
	err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		if (key_elem.type == QD_STACK_TYPE_STR) qd_string_release(key_elem.value.s);
		if (cert_elem.type == QD_STACK_TYPE_STR) qd_string_release(cert_elem.value.s);
		if (hostname_elem.type == QD_STACK_TYPE_STR) qd_string_release(hostname_elem.value.s);
		fprintf(stderr, "Fatal error in tls::connect_mtls: stack underflow\n");
		abort();
	}

	// Type checks
	if (socket_elem.type != QD_STACK_TYPE_INT) {
		if (key_elem.type == QD_STACK_TYPE_STR) qd_string_release(key_elem.value.s);
		if (cert_elem.type == QD_STACK_TYPE_STR) qd_string_release(cert_elem.value.s);
		if (hostname_elem.type == QD_STACK_TYPE_STR) qd_string_release(hostname_elem.value.s);
		fprintf(stderr, "Fatal error in tls::connect_mtls: socket must be an integer\n");
		abort();
	}

	if (hostname_elem.type != QD_STACK_TYPE_STR) {
		if (key_elem.type == QD_STACK_TYPE_STR) qd_string_release(key_elem.value.s);
		if (cert_elem.type == QD_STACK_TYPE_STR) qd_string_release(cert_elem.value.s);
		fprintf(stderr, "Fatal error in tls::connect_mtls: hostname must be a string\n");
		abort();
	}

	if (cert_elem.type != QD_STACK_TYPE_STR) {
		if (key_elem.type == QD_STACK_TYPE_STR) qd_string_release(key_elem.value.s);
		qd_string_release(hostname_elem.value.s);
		fprintf(stderr, "Fatal error in tls::connect_mtls: cert_path must be a string\n");
		abort();
	}

	if (key_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(cert_elem.value.s);
		qd_string_release(hostname_elem.value.s);
		fprintf(stderr, "Fatal error in tls::connect_mtls: key_path must be a string\n");
		abort();
	}

	int socket_fd = (int)socket_elem.value.i;
	const char* hostname = qd_string_data(hostname_elem.value.s);
	const char* cert_path = qd_string_data(cert_elem.value.s);
	const char* key_path = qd_string_data(key_elem.value.s);

	// Perform TLS connect with client certificate
	tls_conn_t conn = TLS_CONN_INVALID;
	tls_error_t tls_err = tls_platform_connect_mtls(socket_fd, hostname, cert_path, key_path, &conn);

	qd_string_release(hostname_elem.value.s);
	qd_string_release(cert_elem.value.s);
	qd_string_release(key_elem.value.s);

	if (tls_err != TLS_OK) {
		// On failure: set error and push only error code
		ctx->error_code = (int)tls_err;
		if (ctx->error_msg) free(ctx->error_msg);
		ctx->error_msg = strdup(tls_platform_error_string(tls_err));
		qd_push_i(ctx, (int64_t)tls_err);
		return (qd_exec_result){(int)tls_err};
	}

	// On success: push result, then Ok
	qd_push_p(ctx, conn);
	qd_push_i(ctx, TLS_OK);
	return (qd_exec_result){0};
}

// Stack signature: ( socket:i cert_path:s key_path:s -- tls_conn:ptr )
// Fallible function - pushes error code
qd_exec_result usr_tls_accept(qd_context* ctx) {
	// Pop key_path
	qd_stack_element_t key_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &key_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in tls::accept: stack underflow\n");
		abort();
	}
	
	// Pop cert_path
	qd_stack_element_t cert_elem;
	err = qd_stack_pop(ctx->st, &cert_elem);
	if (err != QD_STACK_OK) {
		if (key_elem.type == QD_STACK_TYPE_STR) qd_string_release(key_elem.value.s);
		fprintf(stderr, "Fatal error in tls::accept: stack underflow\n");
		abort();
	}
	
	// Pop socket
	qd_stack_element_t socket_elem;
	err = qd_stack_pop(ctx->st, &socket_elem);
	if (err != QD_STACK_OK) {
		if (key_elem.type == QD_STACK_TYPE_STR) qd_string_release(key_elem.value.s);
		if (cert_elem.type == QD_STACK_TYPE_STR) qd_string_release(cert_elem.value.s);
		fprintf(stderr, "Fatal error in tls::accept: stack underflow\n");
		abort();
	}
	
	// Type checks
	if (socket_elem.type != QD_STACK_TYPE_INT) {
		if (key_elem.type == QD_STACK_TYPE_STR) qd_string_release(key_elem.value.s);
		if (cert_elem.type == QD_STACK_TYPE_STR) qd_string_release(cert_elem.value.s);
		fprintf(stderr, "Fatal error in tls::accept: socket must be an integer\n");
		abort();
	}
	
	if (cert_elem.type != QD_STACK_TYPE_STR) {
		if (key_elem.type == QD_STACK_TYPE_STR) qd_string_release(key_elem.value.s);
		fprintf(stderr, "Fatal error in tls::accept: cert_path must be a string\n");
		abort();
	}
	
	if (key_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(cert_elem.value.s);
		fprintf(stderr, "Fatal error in tls::accept: key_path must be a string\n");
		abort();
	}
	
	int socket_fd = (int)socket_elem.value.i;
	const char* cert_path = qd_string_data(cert_elem.value.s);
	const char* key_path = qd_string_data(key_elem.value.s);
	
	// Perform TLS accept
	tls_conn_t conn = TLS_CONN_INVALID;
	tls_error_t tls_err = tls_platform_accept(socket_fd, cert_path, key_path, &conn);

	qd_string_release(cert_elem.value.s);
	qd_string_release(key_elem.value.s);

	if (tls_err != TLS_OK) {
		// On failure: set error and push only error code
		ctx->error_code = (int)tls_err;
		if (ctx->error_msg) free(ctx->error_msg);
		ctx->error_msg = strdup(tls_platform_error_string(tls_err));
		qd_push_i(ctx, (int64_t)tls_err);
		return (qd_exec_result){(int)tls_err};
	}

	// On success: push result, then Ok
	qd_push_p(ctx, conn);
	qd_push_i(ctx, TLS_OK);
	return (qd_exec_result){0};
}

// Stack signature: ( tls_conn:ptr data:s -- bytes_sent:i )
// Fallible function - pushes error code
qd_exec_result usr_tls_send(qd_context* ctx) {
	// Pop data
	qd_stack_element_t data_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &data_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in tls::send: stack underflow\n");
		abort();
	}
	
	// Pop connection
	qd_stack_element_t conn_elem;
	err = qd_stack_pop(ctx->st, &conn_elem);
	if (err != QD_STACK_OK) {
		if (data_elem.type == QD_STACK_TYPE_STR) qd_string_release(data_elem.value.s);
		fprintf(stderr, "Fatal error in tls::send: stack underflow\n");
		abort();
	}
	
	// Type checks
	if (conn_elem.type != QD_STACK_TYPE_PTR) {
		if (data_elem.type == QD_STACK_TYPE_STR) qd_string_release(data_elem.value.s);
		fprintf(stderr, "Fatal error in tls::send: connection must be a pointer\n");
		abort();
	}
	
	if (data_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in tls::send: data must be a string\n");
		abort();
	}
	
	tls_conn_t conn = conn_elem.value.p;
	const char* data = qd_string_data(data_elem.value.s);
	size_t len = strlen(data);
	
	// Send data
	int bytes_sent = 0;
	tls_error_t tls_err = tls_platform_send(conn, data, len, &bytes_sent);

	qd_string_release(data_elem.value.s);

	if (tls_err != TLS_OK) {
		// On failure: set error and push only error code
		ctx->error_code = (int)tls_err;
		if (ctx->error_msg) free(ctx->error_msg);
		ctx->error_msg = strdup(tls_platform_error_string(tls_err));
		qd_push_i(ctx, (int64_t)tls_err);
		return (qd_exec_result){(int)tls_err};
	}

	// On success: push result, then Ok
	qd_push_i(ctx, (int64_t)bytes_sent);
	qd_push_i(ctx, TLS_OK);
	return (qd_exec_result){0};
}

// Stack signature: ( tls_conn:ptr max_bytes:i -- data:s bytes_read:i )
// Fallible function - pushes error code
qd_exec_result usr_tls_receive(qd_context* ctx) {
	// Pop max_bytes
	qd_stack_element_t max_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &max_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in tls::receive: stack underflow\n");
		abort();
	}
	
	// Pop connection
	qd_stack_element_t conn_elem;
	err = qd_stack_pop(ctx->st, &conn_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in tls::receive: stack underflow\n");
		abort();
	}
	
	// Type checks
	if (conn_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in tls::receive: connection must be a pointer\n");
		abort();
	}
	
	if (max_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in tls::receive: max_bytes must be an integer\n");
		abort();
	}
	
	tls_conn_t conn = conn_elem.value.p;
	int max_bytes = (int)max_elem.value.i;
	
	if (max_bytes <= 0 || max_bytes > 1048576) { // Max 1MB
		fprintf(stderr, "Fatal error in tls::receive: max_bytes must be between 1 and 1048576\n");
		abort();
	}
	
	// Allocate buffer
	char* buffer = malloc((size_t)max_bytes + 1);
	if (buffer == NULL) {
		fprintf(stderr, "Fatal error in tls::receive: failed to allocate buffer\n");
		abort();
	}
	
	// Receive data
	int bytes_read = 0;
	tls_error_t tls_err = tls_platform_receive(conn, buffer, (size_t)max_bytes, &bytes_read);

	buffer[bytes_read > 0 ? bytes_read : 0] = '\0';

	if (tls_err != TLS_OK) {
		// On failure: set error and push only error code
		free(buffer);
		ctx->error_code = (int)tls_err;
		if (ctx->error_msg) free(ctx->error_msg);
		ctx->error_msg = strdup(tls_platform_error_string(tls_err));
		qd_push_i(ctx, (int64_t)tls_err);
		return (qd_exec_result){(int)tls_err};
	}

	// On success: push results (data, bytes_read), then Ok
	qd_push_s(ctx, buffer);
	qd_push_i(ctx, (int64_t)bytes_read);
	qd_push_i(ctx, TLS_OK);

	free(buffer);
	return (qd_exec_result){0};
}

// Stack signature: ( tls_conn:ptr -- )
qd_exec_result usr_tls_close(qd_context* ctx) {
	// Pop connection
	qd_stack_element_t conn_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &conn_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in tls::close: stack underflow\n");
		abort();
	}
	
	if (conn_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in tls::close: connection must be a pointer\n");
		abort();
	}
	
	tls_conn_t conn = conn_elem.value.p;
	if (conn != TLS_CONN_INVALID) {
		tls_platform_close(conn);
	}
	
	return (qd_exec_result){0};
}
