/**
 * @file tls.h
 * @brief TLS/SSL operations for Quadrate (tls:: module)
 *
 * Provides TLS encryption layer on top of TCP sockets using OpenSSL.
 */

#ifndef QD_QDTLS_TLS_H
#define QD_QDTLS_TLS_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wrap a socket with TLS (client mode)
 * @par Stack Effect: ( socket:i hostname:s -- tls_conn:ptr )
 * @param ctx Execution context
 * @return Execution result (fallible)
 *
 * Performs TLS handshake as client. Uses hostname for SNI and verification.
 */
qd_exec_result usr_tls_connect(qd_context* ctx);

/**
 * @brief Wrap a socket with TLS using client certificate (mTLS)
 * @par Stack Effect: ( socket:i hostname:s cert_path:s key_path:s -- tls_conn:ptr )
 * @param ctx Execution context
 * @return Execution result (fallible)
 *
 * Performs TLS handshake as client with client certificate authentication.
 * Uses hostname for SNI and verification.
 */
qd_exec_result usr_tls_connect_mtls(qd_context* ctx);

/**
 * @brief Wrap a socket with TLS (server mode)
 * @par Stack Effect: ( socket:i cert_path:s key_path:s -- tls_conn:ptr )
 * @param ctx Execution context
 * @return Execution result (fallible)
 *
 * Performs TLS handshake as server using provided certificate and key.
 */
qd_exec_result usr_tls_accept(qd_context* ctx);

/**
 * @brief Send data over TLS connection
 * @par Stack Effect: ( tls_conn:ptr data:s -- bytes_sent:i )
 * @param ctx Execution context
 * @return Execution result (fallible)
 *
 * Encrypts and sends data over the TLS connection.
 */
qd_exec_result usr_tls_send(qd_context* ctx);

/**
 * @brief Receive data from TLS connection
 * @par Stack Effect: ( tls_conn:ptr max_bytes:i -- data:s bytes_read:i )
 * @param ctx Execution context
 * @return Execution result (fallible)
 *
 * Receives and decrypts data from the TLS connection.
 */
qd_exec_result usr_tls_receive(qd_context* ctx);

/**
 * @brief Close TLS connection
 * @par Stack Effect: ( tls_conn:ptr -- )
 * @param ctx Execution context
 * @return Execution result
 *
 * Performs TLS shutdown and frees resources. Does NOT close underlying socket.
 */
qd_exec_result usr_tls_close(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_QDTLS_TLS_H
