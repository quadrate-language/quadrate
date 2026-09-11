/**
 * @file tls_openssl.c
 * @brief OpenSSL implementation of TLS platform abstraction
 */

#include "../tls_platform.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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

tls_error_t tls_platform_connect(int socket_fd, const char* hostname, tls_conn_t* out_conn) {
	if (out_conn == NULL || hostname == NULL) {
		return TLS_ERR_INVALID_ARG;
	}

	*out_conn = TLS_CONN_INVALID;

	// Ensure initialized
	tls_error_t init_err = tls_platform_init();
	if (init_err != TLS_OK) {
		return init_err;
	}

	// Create SSL context for client
	const SSL_METHOD* method = TLS_client_method();
	SSL_CTX* ctx = SSL_CTX_new(method);
	if (ctx == NULL) {
		return TLS_ERR_INIT;
	}

	// Set minimum TLS version to 1.2 for security
	SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

	// Enable certificate verification
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

	// Load default CA certificates
	if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
		SSL_CTX_free(ctx);
		return TLS_ERR_CERTIFICATE;
	}

	// Create SSL connection
	SSL* ssl = SSL_new(ctx);
	if (ssl == NULL) {
		SSL_CTX_free(ctx);
		return TLS_ERR_INIT;
	}

	// Set Server Name Indication (SNI)
	SSL_set_tlsext_host_name(ssl, hostname);

	// Enable hostname verification
	SSL_set1_host(ssl, hostname);

	// Attach to socket
	if (SSL_set_fd(ssl, socket_fd) != 1) {
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return TLS_ERR_INIT;
	}

	// Perform TLS handshake
	int ret = SSL_connect(ssl);
	if (ret != 1) {
		int ssl_err = SSL_get_error(ssl, ret);
		SSL_free(ssl);
		SSL_CTX_free(ctx);

		if (ssl_err == SSL_ERROR_SSL) {
			// Check if it's a certificate error
			unsigned long err = ERR_peek_error();
			int reason = ERR_GET_REASON(err);
			if (reason == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY ||
				reason == X509_V_ERR_CERT_UNTRUSTED ||
				reason == X509_V_ERR_CERT_HAS_EXPIRED ||
				reason == X509_V_ERR_HOSTNAME_MISMATCH) {
				return TLS_ERR_CERTIFICATE;
			}
		}
		return TLS_ERR_CONNECT;
	}

	// Allocate connection structure
	tls_connection_t* conn = malloc(sizeof(tls_connection_t));
	if (conn == NULL) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return TLS_ERR_MEMORY;
	}

	conn->ssl = ssl;
	conn->ctx = ctx;
	conn->socket_fd = socket_fd;
	conn->is_server = false;

	*out_conn = conn;
	return TLS_OK;
}

tls_error_t tls_platform_connect_mtls(int socket_fd, const char* hostname,
                                      const char* cert_path, const char* key_path,
                                      tls_conn_t* out_conn) {
	if (out_conn == NULL || hostname == NULL || cert_path == NULL || key_path == NULL) {
		return TLS_ERR_INVALID_ARG;
	}

	*out_conn = TLS_CONN_INVALID;

	// Ensure initialized
	tls_error_t init_err = tls_platform_init();
	if (init_err != TLS_OK) {
		return init_err;
	}

	// Create SSL context for client
	const SSL_METHOD* method = TLS_client_method();
	SSL_CTX* ctx = SSL_CTX_new(method);
	if (ctx == NULL) {
		return TLS_ERR_INIT;
	}

	// Set minimum TLS version to 1.2 for security
	SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

	// Enable certificate verification
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

	// Load default CA certificates
	if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
		SSL_CTX_free(ctx);
		return TLS_ERR_CERTIFICATE;
	}

	// Load client certificate
	if (SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM) != 1) {
		SSL_CTX_free(ctx);
		return TLS_ERR_CERTIFICATE;
	}

	// Load client private key
	if (SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) != 1) {
		SSL_CTX_free(ctx);
		return TLS_ERR_CERTIFICATE;
	}

	// Verify private key matches certificate
	if (SSL_CTX_check_private_key(ctx) != 1) {
		SSL_CTX_free(ctx);
		return TLS_ERR_CERTIFICATE;
	}

	// Create SSL connection
	SSL* ssl = SSL_new(ctx);
	if (ssl == NULL) {
		SSL_CTX_free(ctx);
		return TLS_ERR_INIT;
	}

	// Set Server Name Indication (SNI)
	SSL_set_tlsext_host_name(ssl, hostname);

	// Enable hostname verification
	SSL_set1_host(ssl, hostname);

	// Attach to socket
	if (SSL_set_fd(ssl, socket_fd) != 1) {
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return TLS_ERR_INIT;
	}

	// Perform TLS handshake
	int ret = SSL_connect(ssl);
	if (ret != 1) {
		int ssl_err = SSL_get_error(ssl, ret);
		SSL_free(ssl);
		SSL_CTX_free(ctx);

		if (ssl_err == SSL_ERROR_SSL) {
			// Check if it's a certificate error
			unsigned long err = ERR_peek_error();
			int reason = ERR_GET_REASON(err);
			if (reason == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY ||
				reason == X509_V_ERR_CERT_UNTRUSTED ||
				reason == X509_V_ERR_CERT_HAS_EXPIRED ||
				reason == X509_V_ERR_HOSTNAME_MISMATCH) {
				return TLS_ERR_CERTIFICATE;
			}
		}
		return TLS_ERR_CONNECT;
	}

	// Allocate connection structure
	tls_connection_t* conn = malloc(sizeof(tls_connection_t));
	if (conn == NULL) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return TLS_ERR_MEMORY;
	}

	conn->ssl = ssl;
	conn->ctx = ctx;
	conn->socket_fd = socket_fd;
	conn->is_server = false;

	*out_conn = conn;
	return TLS_OK;
}

tls_error_t tls_platform_accept(int socket_fd, const char* cert_path, const char* key_path, tls_conn_t* out_conn) {
	if (out_conn == NULL || cert_path == NULL || key_path == NULL) {
		return TLS_ERR_INVALID_ARG;
	}

	*out_conn = TLS_CONN_INVALID;

	// Ensure initialized
	tls_error_t init_err = tls_platform_init();
	if (init_err != TLS_OK) {
		return init_err;
	}

	// Create SSL context for server
	const SSL_METHOD* method = TLS_server_method();
	SSL_CTX* ctx = SSL_CTX_new(method);
	if (ctx == NULL) {
		return TLS_ERR_INIT;
	}

	// Set minimum TLS version to 1.2 for security
	SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

	// Load certificate
	if (SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM) != 1) {
		SSL_CTX_free(ctx);
		return TLS_ERR_CERTIFICATE;
	}

	// Load private key
	if (SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) != 1) {
		SSL_CTX_free(ctx);
		return TLS_ERR_CERTIFICATE;
	}

	// Verify private key matches certificate
	if (SSL_CTX_check_private_key(ctx) != 1) {
		SSL_CTX_free(ctx);
		return TLS_ERR_CERTIFICATE;
	}

	// Create SSL connection
	SSL* ssl = SSL_new(ctx);
	if (ssl == NULL) {
		SSL_CTX_free(ctx);
		return TLS_ERR_INIT;
	}

	// Attach to socket
	if (SSL_set_fd(ssl, socket_fd) != 1) {
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return TLS_ERR_INIT;
	}

	// Perform TLS handshake (server side)
	int ret = SSL_accept(ssl);
	if (ret != 1) {
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return TLS_ERR_ACCEPT;
	}

	// Allocate connection structure
	tls_connection_t* conn = malloc(sizeof(tls_connection_t));
	if (conn == NULL) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return TLS_ERR_MEMORY;
	}

	conn->ssl = ssl;
	conn->ctx = ctx;
	conn->socket_fd = socket_fd;
	conn->is_server = true;

	*out_conn = conn;
	return TLS_OK;
}

tls_error_t tls_platform_send(tls_conn_t conn, const char* data, size_t len, int* out_sent) {
	if (conn == TLS_CONN_INVALID || data == NULL || out_sent == NULL) {
		return TLS_ERR_INVALID_ARG;
	}

	tls_connection_t* c = (tls_connection_t*)conn;

	int sent = SSL_write(c->ssl, data, (int)len);
	if (sent <= 0) {
		int ssl_err = SSL_get_error(c->ssl, sent);
		if (ssl_err == SSL_ERROR_ZERO_RETURN) {
			return TLS_ERR_CLOSED;
		}
		return TLS_ERR_WRITE;
	}

	*out_sent = sent;
	return TLS_OK;
}

tls_error_t tls_platform_receive(tls_conn_t conn, char* buffer, size_t max_len, int* out_received) {
	if (conn == TLS_CONN_INVALID || buffer == NULL || out_received == NULL) {
		return TLS_ERR_INVALID_ARG;
	}

	tls_connection_t* c = (tls_connection_t*)conn;

	int received = SSL_read(c->ssl, buffer, (int)max_len);
	if (received <= 0) {
		int ssl_err = SSL_get_error(c->ssl, received);
		if (ssl_err == SSL_ERROR_ZERO_RETURN) {
			*out_received = 0;
			return TLS_ERR_CLOSED;
		}
		return TLS_ERR_READ;
	}

	*out_received = received;
	return TLS_OK;
}

void tls_platform_close(tls_conn_t conn) {
	if (conn == TLS_CONN_INVALID) {
		return;
	}

	tls_connection_t* c = (tls_connection_t*)conn;

	// Send TLS shutdown alert
	SSL_shutdown(c->ssl);

	// Free SSL resources
	SSL_free(c->ssl);
	SSL_CTX_free(c->ctx);

	// Free connection structure
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
