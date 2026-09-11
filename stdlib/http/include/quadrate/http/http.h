/**
 * @file http.h
 * @brief HTTP client and server module for Quadrate
 *
 * Provides HTTP/HTTPS client and server functionality.
 */

#ifndef QD_HTTP_H
#define QD_HTTP_H

#include <quadrate/rt/runtime.h>
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

/** Server error codes */
#define HTTP_ERR_BIND 20
#define HTTP_ERR_ACCEPT 21
#define HTTP_ERR_PARSE_REQUEST 22

/** Maximum header size */
#define HTTP_MAX_HEADERS 8192

/** Maximum URL length */
#define HTTP_MAX_URL 4096

/** Maximum routes per engine */
#define HTTP_MAX_ROUTES 256

/** Maximum middleware per engine/group */
#define HTTP_MAX_MIDDLEWARE 32

/** Maximum route groups */
#define HTTP_MAX_GROUPS 64

/**
 * @brief Create new HTTP request
 * Stack: ( url:str -- req:ptr )
 */
int usr_http_new(qd_context* ctx);

/**
 * @brief Set request method
 * Stack: ( req:ptr method:str -- )
 */
int usr_http_method(qd_context* ctx);

/**
 * @brief Add request header
 * Stack: ( req:ptr name:str value:str -- )
 */
int usr_http_header(qd_context* ctx);

/**
 * @brief Set request body
 * Stack: ( req:ptr body:str -- )
 */
int usr_http_body(qd_context* ctx);

/**
 * @brief Set client certificate for mTLS
 * Stack: ( req:ptr cert_path:str key_path:str -- )
 */
int usr_http_cert(qd_context* ctx);

/**
 * @brief Execute HTTP request
 * Stack: ( req:ptr -- resp:ptr )
 * Fallible - pushes error code
 */
int usr_http_send(qd_context* ctx);

/**
 * @brief Close and free response
 * Stack: ( resp:ptr -- )
 */
int usr_http_close(qd_context* ctx);

/**
 * @brief Simple GET request
 * Stack: ( url:str -- resp:ptr )
 * Fallible - pushes error code
 */
int usr_http_get(qd_context* ctx);

/**
 * @brief Free request object
 * Stack: ( req:ptr -- )
 */
int usr_http_free_request(qd_context* ctx);

// ============================================================
// Server API
// ============================================================

/**
 * @brief Create new HTTP engine
 * Stack: ( -- engine:ptr )
 */
int usr_http_engine(qd_context* ctx);

/**
 * @brief Register GET route
 * Stack: ( engine:ptr path:str handler:fn -- )
 */
int usr_http_GET(qd_context* ctx);

/**
 * @brief Register POST route
 * Stack: ( engine:ptr path:str handler:fn -- )
 */
int usr_http_POST(qd_context* ctx);

/**
 * @brief Register PUT route
 * Stack: ( engine:ptr path:str handler:fn -- )
 */
int usr_http_PUT(qd_context* ctx);

/**
 * @brief Register DELETE route
 * Stack: ( engine:ptr path:str handler:fn -- )
 */
int usr_http_DELETE(qd_context* ctx);

/**
 * @brief Register handler for any method
 * Stack: ( engine:ptr path:str handler:fn -- )
 */
int usr_http_ANY(qd_context* ctx);

/**
 * @brief Add middleware to engine
 * Stack: ( engine:ptr middleware:fn -- )
 */
int usr_http_use(qd_context* ctx);

/**
 * @brief Start server (blocking)
 * Stack: ( engine:ptr addr:str -- )!
 */
int usr_http_run(qd_context* ctx);

/**
 * @brief Handle single request (non-blocking)
 * Stack: ( engine:ptr -- handled:i64 )
 */
int usr_http_handle_one(qd_context* ctx);

/**
 * @brief Stop the server
 * Stack: ( engine:ptr -- )
 */
int usr_http_stop(qd_context* ctx);

/**
 * @brief Free engine resources
 * Stack: ( engine:ptr -- )
 */
int usr_http_free_engine(qd_context* ctx);

// ============================================================
// Context Methods
// ============================================================

/**
 * @brief Get path parameter by name
 * Stack: ( ctx:Ctx name:str -- value:str )
 */
int usr_http_param(qd_context* ctx);

/**
 * @brief Get query parameter by name
 * Stack: ( ctx:Ctx name:str -- value:str )
 */
int usr_http_query_param(qd_context* ctx);

/**
 * @brief Get header value by name
 * Stack: ( ctx:Ctx name:str -- value:str )
 */
int usr_http_get_header(qd_context* ctx);

/**
 * @brief Send plain text response
 * Stack: ( ctx:Ctx status:i64 body:str -- )
 */
int usr_http_string(qd_context* ctx);

/**
 * @brief Send JSON response
 * Stack: ( ctx:Ctx status:i64 body:str -- )
 */
int usr_http_json(qd_context* ctx);

/**
 * @brief Send HTML response
 * Stack: ( ctx:Ctx status:i64 body:str -- )
 */
int usr_http_html(qd_context* ctx);

/**
 * @brief Set response header
 * Stack: ( ctx:Ctx name:str value:str -- )
 */
int usr_http_set_header(qd_context* ctx);

/**
 * @brief Abort request and send error
 * Stack: ( ctx:Ctx status:i64 body:str -- )
 */
int usr_http_abort(qd_context* ctx);

/**
 * @brief Check if response was sent
 * Stack: ( ctx:Ctx -- sent:i64 )
 */
int usr_http_is_responded(qd_context* ctx);

// ============================================================
// Route Groups
// ============================================================

/**
 * @brief Create route group with prefix
 * Stack: ( engine:ptr prefix:str -- group:ptr )
 */
int usr_http_group(qd_context* ctx);

/**
 * @brief Add middleware to group
 * Stack: ( group:ptr middleware:fn -- )
 */
int usr_http_group_use(qd_context* ctx);

/**
 * @brief Register GET on group
 * Stack: ( group:ptr path:str handler:fn -- )
 */
int usr_http_group_GET(qd_context* ctx);

/**
 * @brief Register POST on group
 * Stack: ( group:ptr path:str handler:fn -- )
 */
int usr_http_group_POST(qd_context* ctx);

/**
 * @brief Register PUT on group
 * Stack: ( group:ptr path:str handler:fn -- )
 */
int usr_http_group_PUT(qd_context* ctx);

/**
 * @brief Register DELETE on group
 * Stack: ( group:ptr path:str handler:fn -- )
 */
int usr_http_group_DELETE(qd_context* ctx);

// ============================================================
// Static File Serving
// ============================================================

/**
 * @brief Serve a single file
 * Stack: ( ctx:Ctx filepath:str -- )
 */
int usr_http_static_file(qd_context* ctx);

/**
 * @brief Serve files from directory (used in handler)
 * Maps request path to filesystem. Must be called in a route handler.
 * Stack: ( ctx:Ctx prefix:str fs_path:str -- )
 */
int usr_http_static(qd_context* ctx);

// ============================================================
// Server-Sent Events (SSE)
// ============================================================

/**
 * @brief Start SSE stream
 * Sends SSE headers and keeps connection open.
 * Stack: ( ctx:Ctx -- )
 */
int usr_http_sse_start(qd_context* ctx);

/**
 * @brief Send SSE data event
 * Sends a data-only event to the client.
 * Stack: ( ctx:Ctx data:str -- )
 */
int usr_http_sse_send(qd_context* ctx);

/**
 * @brief Send named SSE event
 * Sends an event with a name and data.
 * Stack: ( ctx:Ctx name:str data:str -- )
 */
int usr_http_sse_event(qd_context* ctx);

/**
 * @brief End SSE stream
 * Marks the SSE stream as complete.
 * Stack: ( ctx:Ctx -- )
 */
int usr_http_sse_end(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_HTTP_H
