/**
 * @file net.h
 * @brief Network socket operations for Quadrate (net:: module)
 *
 * Provides TCP socket functionality for network programming.
 */

#ifndef STDQD_NET_H
#define STDQD_NET_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and bind a listening socket
 * @par Stack Effect: ( port:i -- socket_fd:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Creates a TCP socket, binds it to the specified port, and starts listening.
 */
qd_exec_result usr_net_listen(qd_context* ctx);

/**
 * @brief Accept an incoming connection
 * @par Stack Effect: ( listen_fd:i -- client_fd:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Blocks until a client connects, then returns the client socket descriptor.
 */
qd_exec_result usr_net_accept(qd_context* ctx);

/**
 * @brief Connect to a remote server
 * @par Stack Effect: ( host:s port:i -- socket_fd:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Connects to the specified host and port, returns the socket descriptor.
 */
qd_exec_result usr_net_connect(qd_context* ctx);

/**
 * @brief Send data over a socket
 * @par Stack Effect: ( socket_fd:i data:s -- bytes_sent:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Sends the string data over the socket.
 */
qd_exec_result usr_net_send(qd_context* ctx);

/**
 * @brief Receive data from a socket
 * @par Stack Effect: ( socket_fd:i max_bytes:i -- data:s )
 * @param ctx Execution context
 * @return Execution result
 *
 * Receives up to max_bytes from the socket, returns the received data as a string.
 */
qd_exec_result usr_net_receive(qd_context* ctx);

/**
 * @brief Shutdown a socket
 * @par Stack Effect: ( socket_fd:i -- )
 * @param ctx Execution context
 * @return Execution result
 *
 * Shuts down the socket for further send/receive operations.
 */
qd_exec_result usr_net_shutdown(qd_context* ctx);

/**
 * @brief Close a socket
 * @par Stack Effect: ( socket_fd:i -- )
 * @param ctx Execution context
 * @return Execution result
 *
 * Closes the socket and releases associated resources.
 */
qd_exec_result usr_net_close(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // STDQD_NET_H
