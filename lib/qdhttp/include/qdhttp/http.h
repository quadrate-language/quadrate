/**
 * @file http.h
 * @brief HTTP client module for Quadrate
 *
 * Provides HTTP/HTTPS client functionality using net and tls modules.
 */

#ifndef QD_HTTP_H
#define QD_HTTP_H

#include <qdrt/runtime.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Error codes - Ok=1, errors start at 2 */
#define HTTP_OK 1
#define HTTP_ERR_CONNECT 2
#define HTTP_ERR_TLS 3
#define HTTP_ERR_TIMEOUT 4
#define HTTP_ERR_PARSE 5
#define HTTP_ERR_INVALID_URL 6
#define HTTP_ERR_REDIRECT 7
#define HTTP_ERR_MEMORY 8
#define HTTP_ERR_SEND 9
#define HTTP_ERR_RECEIVE 10

/** Maximum header size */
#define HTTP_MAX_HEADERS 8192

/** Maximum URL length */
#define HTTP_MAX_URL 4096

/**
 * @brief Create new HTTP request
 * Stack: ( url:str -- req:ptr )
 */
qd_exec_result usr_http_new(qd_context* ctx);

/**
 * @brief Set request method
 * Stack: ( req:ptr method:str -- )
 */
qd_exec_result usr_http_method(qd_context* ctx);

/**
 * @brief Add request header
 * Stack: ( req:ptr name:str value:str -- )
 */
qd_exec_result usr_http_header(qd_context* ctx);

/**
 * @brief Set request body
 * Stack: ( req:ptr body:str -- )
 */
qd_exec_result usr_http_body(qd_context* ctx);

/**
 * @brief Execute HTTP request
 * Stack: ( req:ptr -- resp:ptr )
 * Fallible - pushes error code
 */
qd_exec_result usr_http_send(qd_context* ctx);

/**
 * @brief Close and free response
 * Stack: ( resp:ptr -- )
 */
qd_exec_result usr_http_close(qd_context* ctx);

/**
 * @brief Simple GET request
 * Stack: ( url:str -- resp:ptr )
 * Fallible - pushes error code
 */
qd_exec_result usr_http_get(qd_context* ctx);

/**
 * @brief Free request object
 * Stack: ( req:ptr -- )
 */
qd_exec_result usr_http_free_request(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_HTTP_H
