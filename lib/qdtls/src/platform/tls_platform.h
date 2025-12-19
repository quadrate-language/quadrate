/**
 * @file tls_platform.h
 * @brief Platform abstraction layer for TLS operations
 *
 * This header defines the interface that platform-specific TLS implementations
 * must provide. Currently supports OpenSSL.
 */

#ifndef QD_TLS_PLATFORM_H
#define QD_TLS_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque TLS connection handle */
typedef void* tls_conn_t;

/** Invalid TLS connection */
#define TLS_CONN_INVALID NULL

/** TLS error codes - Ok=1 matches Quadrate builtin, errors start at 2 */
typedef enum {
	TLS_OK = 1,				  // Success (matches Quadrate Ok)
	TLS_ERR_INIT = 2,		  // Failed to initialize TLS library
	TLS_ERR_CONNECT = 3,	  // TLS handshake failed (client)
	TLS_ERR_ACCEPT = 4,		  // TLS handshake failed (server)
	TLS_ERR_CERTIFICATE = 5,  // Certificate validation failed
	TLS_ERR_READ = 6,		  // Read error
	TLS_ERR_WRITE = 7,		  // Write error
	TLS_ERR_CLOSED = 8,		  // Connection closed by peer
	TLS_ERR_MEMORY = 9,		  // Memory allocation failed
	TLS_ERR_INVALID_ARG = 10, // Invalid argument
} tls_error_t;

/**
 * @brief Initialize TLS library (call once at startup)
 * @return TLS_OK on success, error code otherwise
 */
tls_error_t tls_platform_init(void);

/**
 * @brief Cleanup TLS library (call once at shutdown)
 */
void tls_platform_cleanup(void);

/**
 * @brief Create TLS client connection
 * @param socket_fd Underlying TCP socket file descriptor
 * @param hostname Server hostname for SNI and verification
 * @param out_conn Output: TLS connection handle
 * @return TLS_OK on success, error code otherwise
 */
tls_error_t tls_platform_connect(int socket_fd, const char* hostname, tls_conn_t* out_conn);

/**
 * @brief Create TLS client connection with client certificate (mTLS)
 * @param socket_fd Underlying TCP socket file descriptor
 * @param hostname Server hostname for SNI and verification
 * @param cert_path Path to PEM client certificate file
 * @param key_path Path to PEM client private key file
 * @param out_conn Output: TLS connection handle
 * @return TLS_OK on success, error code otherwise
 */
tls_error_t tls_platform_connect_mtls(
		int socket_fd, const char* hostname, const char* cert_path, const char* key_path, tls_conn_t* out_conn);

/**
 * @brief Create TLS server connection
 * @param socket_fd Underlying TCP socket file descriptor
 * @param cert_path Path to PEM certificate file
 * @param key_path Path to PEM private key file
 * @param out_conn Output: TLS connection handle
 * @return TLS_OK on success, error code otherwise
 */
tls_error_t tls_platform_accept(int socket_fd, const char* cert_path, const char* key_path, tls_conn_t* out_conn);

/**
 * @brief Send data over TLS connection
 * @param conn TLS connection handle
 * @param data Data to send
 * @param len Length of data
 * @param out_sent Output: bytes actually sent
 * @return TLS_OK on success, error code otherwise
 */
tls_error_t tls_platform_send(tls_conn_t conn, const char* data, size_t len, int* out_sent);

/**
 * @brief Receive data from TLS connection
 * @param conn TLS connection handle
 * @param buffer Buffer to receive into
 * @param max_len Maximum bytes to receive
 * @param out_received Output: bytes actually received
 * @return TLS_OK on success, TLS_ERR_CLOSED on EOF, error code otherwise
 */
tls_error_t tls_platform_receive(tls_conn_t conn, char* buffer, size_t max_len, int* out_received);

/**
 * @brief Close TLS connection and free resources
 * @param conn TLS connection handle
 *
 * Performs TLS shutdown. Does NOT close underlying socket.
 */
void tls_platform_close(tls_conn_t conn);

/**
 * @brief Get human-readable error message
 * @param err Error code
 * @return Static string describing the error
 */
const char* tls_platform_error_string(tls_error_t err);

#ifdef __cplusplus
}
#endif

#endif // QD_TLS_PLATFORM_H
