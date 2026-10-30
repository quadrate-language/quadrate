/**
 * @file tls_openssl.c
 * @brief OpenSSL implementation of TLS platform abstraction
 */

#define _POSIX_C_SOURCE 200809L

#include "../tls_platform.h"
#include <errno.h>
#include <limits.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** TLS connection structure */
typedef struct {
	SSL* ssl;
	SSL_CTX* ctx;
	int socket_fd;
	bool is_server;
} tls_connection_t;

/** Global initialization flag */
static bool g_initialized = false;

tls_error_t tls_platform_init(void) {
	if (g_initialized) {
		return TLS_OK;
	}

	// OpenSSL 1.1.0+ auto-initializes, but we call explicitly for clarity
	OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);

	g_initialized = true;
	return TLS_OK;
}

void tls_platform_cleanup(void) {
	if (!g_initialized) {
		return;
	}
	// OpenSSL 1.1.0+ handles cleanup automatically
	g_initialized = false;
}

// Why the last operation on this thread failed, in OpenSSL's words
static _Thread_local char g_detail[256];

static tls_error_t fail(tls_error_t err, const char* detail) {
	snprintf(g_detail, sizeof(g_detail), "%s", detail ? detail : "");
	ERR_clear_error();
	return err;
}

// The reason OpenSSL queued for the failure, or `otherwise`
static tls_error_t fail_openssl(tls_error_t err, const char* otherwise) {
	const unsigned long code = ERR_peek_last_error();
	const char* reason = code ? ERR_reason_error_string(code) : NULL;
	return fail(err, reason ? reason : otherwise);
}

const char* tls_platform_error_detail(void) {
	return g_detail;
}

// SIGPIPE. OpenSSL writes to the socket with plain write(), and on Linux there is no socket
// option against SIGPIPE, so a write to a peer that has gone ends the whole process. Block
// the signal in this thread around each call that may write, and take back one the call
// raised, so the write fails with EPIPE instead.
typedef struct {
	sigset_t old_mask;
	bool was_pending;
} sigpipe_guard_t;

static void sigpipe_block(sigpipe_guard_t* g) {
	sigset_t pending;
	sigemptyset(&pending);
	sigpending(&pending);
	g->was_pending = sigismember(&pending, SIGPIPE) == 1;
	sigset_t block;
	sigemptyset(&block);
	sigaddset(&block, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &block, &g->old_mask);
}

static void sigpipe_restore(sigpipe_guard_t* g) {
	sigset_t pending;
	sigemptyset(&pending);
	sigpending(&pending);
	if (!g->was_pending && sigismember(&pending, SIGPIPE) == 1) {
		sigset_t pipe_only;
		sigemptyset(&pipe_only);
		sigaddset(&pipe_only, SIGPIPE);
		const struct timespec now = {0, 0};
		while (sigtimedwait(&pipe_only, NULL, &now) < 0 && errno == EINTR) {
		}
	}
	pthread_sigmask(SIG_SETMASK, &g->old_mask, NULL);
}

static tls_error_t wrap(SSL* ssl, SSL_CTX* ctx, int socket_fd, bool is_server, tls_conn_t* out_conn) {
	tls_connection_t* conn = malloc(sizeof(tls_connection_t));
	if (conn == NULL) {
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return fail(TLS_ERR_MEMORY, "out of memory");
	}
	conn->ssl = ssl;
	conn->ctx = ctx;
	conn->socket_fd = socket_fd;
	conn->is_server = is_server;
	*out_conn = conn;
	return TLS_OK;
}

// Load a certificate and its key into `ctx`
static tls_error_t load_identity(SSL_CTX* ctx, const char* cert_path, const char* key_path) {
	if (SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM) != 1) {
		return fail_openssl(TLS_ERR_CERTIFICATE, "could not load the certificate");
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) != 1) {
		return fail_openssl(TLS_ERR_CERTIFICATE, "could not load the private key");
	}
	if (SSL_CTX_check_private_key(ctx) != 1) {
		return fail(TLS_ERR_CERTIFICATE, "the private key does not match the certificate");
	}
	return TLS_OK;
}

// The client side, with a client certificate when `cert_path` is not NULL
static tls_error_t client_connect(
		int socket_fd, const char* hostname, const char* cert_path, const char* key_path, tls_conn_t* out_conn) {
	if (out_conn == NULL || hostname == NULL) {
		return fail(TLS_ERR_INVALID_ARG, "invalid argument");
	}
	*out_conn = TLS_CONN_INVALID;
	tls_error_t err = tls_platform_init();
	if (err != TLS_OK) {
		return fail(err, "could not initialise OpenSSL");
	}

	SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
	if (ctx == NULL) {
		return fail_openssl(TLS_ERR_INIT, "could not create the TLS context");
	}
	// Set minimum TLS version to 1.2 for security
	SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
	// The system's CA certificates, or those SSL_CERT_FILE and SSL_CERT_DIR name
	if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
		SSL_CTX_free(ctx);
		return fail_openssl(TLS_ERR_CERTIFICATE, "could not load the CA certificates");
	}
	if (cert_path != NULL && (err = load_identity(ctx, cert_path, key_path)) != TLS_OK) {
		SSL_CTX_free(ctx);
		return err;
	}

	SSL* ssl = SSL_new(ctx);
	if (ssl == NULL) {
		SSL_CTX_free(ctx);
		return fail_openssl(TLS_ERR_INIT, "could not create the TLS session");
	}
	// SNI, and the name the certificate must carry
	SSL_set_tlsext_host_name(ssl, hostname);
	SSL_set1_host(ssl, hostname);
	if (SSL_set_fd(ssl, socket_fd) != 1) {
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return fail(TLS_ERR_INIT, "not a socket");
	}

	sigpipe_guard_t guard;
	sigpipe_block(&guard);
	const int ret = SSL_connect(ssl);
	sigpipe_restore(&guard);
	if (ret != 1) {
		// A certificate that does not verify fails the handshake like anything else;
		// the verify result is what tells them apart
		const long verify = SSL_get_verify_result(ssl);
		if (verify != X509_V_OK) {
			err = fail(TLS_ERR_CERTIFICATE, X509_verify_cert_error_string(verify));
		} else {
			err = fail_openssl(TLS_ERR_CONNECT, "the handshake failed");
		}
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return err;
	}
	return wrap(ssl, ctx, socket_fd, false, out_conn);
}

tls_error_t tls_platform_connect(int socket_fd, const char* hostname, tls_conn_t* out_conn) {
	return client_connect(socket_fd, hostname, NULL, NULL, out_conn);
}

tls_error_t tls_platform_connect_mtls(
		int socket_fd, const char* hostname, const char* cert_path, const char* key_path, tls_conn_t* out_conn) {
	if (cert_path == NULL || key_path == NULL) {
		return fail(TLS_ERR_INVALID_ARG, "invalid argument");
	}
	return client_connect(socket_fd, hostname, cert_path, key_path, out_conn);
}

tls_error_t tls_platform_accept(int socket_fd, const char* cert_path, const char* key_path, tls_conn_t* out_conn) {
	if (out_conn == NULL || cert_path == NULL || key_path == NULL) {
		return fail(TLS_ERR_INVALID_ARG, "invalid argument");
	}
	*out_conn = TLS_CONN_INVALID;
	tls_error_t err = tls_platform_init();
	if (err != TLS_OK) {
		return fail(err, "could not initialise OpenSSL");
	}

	SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
	if (ctx == NULL) {
		return fail_openssl(TLS_ERR_INIT, "could not create the TLS context");
	}
	SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
	if ((err = load_identity(ctx, cert_path, key_path)) != TLS_OK) {
		SSL_CTX_free(ctx);
		return err;
	}

	SSL* ssl = SSL_new(ctx);
	if (ssl == NULL) {
		SSL_CTX_free(ctx);
		return fail_openssl(TLS_ERR_INIT, "could not create the TLS session");
	}
	if (SSL_set_fd(ssl, socket_fd) != 1) {
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return fail(TLS_ERR_INIT, "not a socket");
	}

	sigpipe_guard_t guard;
	sigpipe_block(&guard);
	const int ret = SSL_accept(ssl);
	sigpipe_restore(&guard);
	if (ret != 1) {
		err = fail_openssl(TLS_ERR_ACCEPT, "the handshake failed");
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return err;
	}
	return wrap(ssl, ctx, socket_fd, true, out_conn);
}

tls_error_t tls_platform_send(tls_conn_t conn, const char* data, size_t len, int* out_sent) {
	if (conn == TLS_CONN_INVALID || data == NULL || out_sent == NULL) {
		return fail(TLS_ERR_INVALID_ARG, "invalid argument");
	}
	tls_connection_t* c = (tls_connection_t*)conn;
	// SSL_write takes an int; a blocking one writes all of what it is given
	const int chunk = len > INT_MAX ? INT_MAX : (int)len;
	sigpipe_guard_t guard;
	sigpipe_block(&guard);
	const int sent = SSL_write(c->ssl, data, chunk);
	sigpipe_restore(&guard);
	if (sent <= 0) {
		const int ssl_err = SSL_get_error(c->ssl, sent);
		if (ssl_err == SSL_ERROR_ZERO_RETURN) {
			return fail(TLS_ERR_CLOSED, "the peer closed the connection");
		}
		return fail_openssl(TLS_ERR_WRITE, ssl_err == SSL_ERROR_SYSCALL ? strerror(errno) : "write failed");
	}
	*out_sent = sent;
	return TLS_OK;
}

tls_error_t tls_platform_receive(tls_conn_t conn, char* buffer, size_t max_len, int* out_received) {
	if (conn == TLS_CONN_INVALID || buffer == NULL || out_received == NULL) {
		return fail(TLS_ERR_INVALID_ARG, "invalid argument");
	}
	tls_connection_t* c = (tls_connection_t*)conn;
	const int received = SSL_read(c->ssl, buffer, max_len > INT_MAX ? INT_MAX : (int)max_len);
	if (received <= 0) {
		const int ssl_err = SSL_get_error(c->ssl, received);
		*out_received = 0;
		if (ssl_err == SSL_ERROR_ZERO_RETURN) {
			return fail(TLS_ERR_CLOSED, "the peer closed the connection");
		}
		return fail_openssl(TLS_ERR_READ, ssl_err == SSL_ERROR_SYSCALL ? "the connection ended without a TLS close"
																		: "read failed");
	}
	*out_received = received;
	return TLS_OK;
}

void tls_platform_close(tls_conn_t conn) {
	if (conn == TLS_CONN_INVALID) {
		return;
	}
	tls_connection_t* c = (tls_connection_t*)conn;
	// The close alert is a write too
	sigpipe_guard_t guard;
	sigpipe_block(&guard);
	SSL_shutdown(c->ssl);
	sigpipe_restore(&guard);
	SSL_free(c->ssl);
	SSL_CTX_free(c->ctx);
	free(c);
}

const char* tls_platform_error_string(tls_error_t err) {
	switch (err) {
		case TLS_OK:              return "Success";
		case TLS_ERR_INIT:        return "TLS initialization failed";
		case TLS_ERR_CONNECT:     return "TLS handshake failed (client)";
		case TLS_ERR_ACCEPT:      return "TLS handshake failed (server)";
		case TLS_ERR_CERTIFICATE: return "Certificate error";
		case TLS_ERR_READ:        return "TLS read error";
		case TLS_ERR_WRITE:       return "TLS write error";
		case TLS_ERR_CLOSED:      return "Connection closed";
		case TLS_ERR_MEMORY:      return "Memory allocation failed";
		case TLS_ERR_INVALID_ARG: return "Invalid argument";
		default:                  return "Unknown TLS error";
	}
}
