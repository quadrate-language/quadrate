/**
 * @file net.h
 * @brief Network socket operations for Quadrate (net:: module)
 *
 * Provides TCP socket functionality for network programming.
 */

#ifndef QD_QDNET_NET_H
#define QD_QDNET_NET_H

#include <quadrate/rt/context.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and bind a listening socket
 * @par Stack Effect: ( port:i -- socket_fd:i )
 * @param ctx Execution context
 * @return 0 on success, non-zero on error
 *
 * Creates a TCP socket, binds it to the specified port, and starts listening.
 */
int usr_net_listen(qd_context* ctx);

/**
 * @brief Accept an incoming connection
 * @par Stack Effect: ( listen_fd:i -- client_fd:i )
 * @param ctx Execution context
 * @return 0 on success, non-zero on error
 *
 * Blocks until a client connects, then returns the client socket descriptor.
 */
int usr_net_accept(qd_context* ctx);

/**
 * @brief Connect to a remote server
 * @par Stack Effect: ( host:s port:i -- socket_fd:i )
 * @param ctx Execution context
 * @return 0 on success, non-zero on error
 *
 * Connects to the specified host and port, returns the socket descriptor.
 */
int usr_net_connect(qd_context* ctx);

/**
 * @brief Send data over a socket
 * @par Stack Effect: ( socket_fd:i data:s -- bytes_sent:i )
 * @param ctx Execution context
 * @return 0 on success, non-zero on error
 *
 * Sends the string data over the socket.
 */
int usr_net_send(qd_context* ctx);

/**
 * @brief Receive data from a socket
 * @par Stack Effect: ( socket_fd:i max_bytes:i -- data:s )
 * @param ctx Execution context
 * @return 0 on success, non-zero on error
 *
 * Receives up to max_bytes from the socket, returns the received data as a string.
 */
int usr_net_receive(qd_context* ctx);

/**
 * @brief Shutdown a socket
 * @par Stack Effect: ( socket_fd:i -- )
 * @param ctx Execution context
 * @return 0 on success, non-zero on error
 *
 * Shuts down the socket for further send/receive operations.
 */
int usr_net_shutdown(qd_context* ctx);

/**
 * @brief Close a socket
 * @par Stack Effect: ( socket_fd:i -- )
 * @param ctx Execution context
 * @return 0 on success, non-zero on error
 *
 * Closes the socket and releases associated resources.
 */
int usr_net_close(qd_context* ctx);

/** Set send/receive timeout on socket. */
int usr_net_set_timeout(qd_context* ctx);

/** Enable/disable TCP keepalive. */
int usr_net_set_keepalive(qd_context* ctx);

/** DNS lookup: resolve hostname to IP address. */
int usr_net_lookup(qd_context* ctx);

/** Get remote address and port of connected socket. */
int usr_net_get_peer_addr(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_QDNET_NET_H
