/**
 * @file http.c
 * @brief HTTP client implementation for Quadrate
 */

#define _POSIX_C_SOURCE 200809L

#include <qdhttp/http.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/** HTTP Request structure */
typedef struct {
	char* url;
	char* method;
	char* headers;
	size_t headers_len;
	size_t headers_cap;
	char* body;
	char* client_cert;  // Path to client certificate for mTLS
	char* client_key;   // Path to client private key for mTLS
} http_request_t;

/** HTTP Response structure - matches Quadrate struct layout */
typedef struct {
	int64_t status;
	struct qd_string* headers;
	struct qd_string* body;
} http_response_t;

/** URL components */
typedef struct {
	bool is_https;
	char host[256];
	int port;
	char path[HTTP_MAX_URL];
} url_parts_t;

/** Parse URL into components */
static int parse_url(const char* url, url_parts_t* parts) {
	memset(parts, 0, sizeof(url_parts_t));
	parts->port = 80;
	strcpy(parts->path, "/");

	const char* p = url;

	// Check scheme
	if (strncmp(p, "https://", 8) == 0) {
		parts->is_https = true;
		parts->port = 443;
		p += 8;
	} else if (strncmp(p, "http://", 7) == 0) {
		parts->is_https = false;
		p += 7;
	} else {
		return HTTP_ERR_INVALID_URL;
	}

	// Extract host (and optional port)
	const char* host_start = p;
	const char* host_end = p;
	while (*host_end && *host_end != '/' && *host_end != ':' && *host_end != '?') {
		host_end++;
	}

	size_t host_len = (size_t)(host_end - host_start);
	if (host_len == 0 || host_len >= sizeof(parts->host)) {
		return HTTP_ERR_INVALID_URL;
	}
	memcpy(parts->host, host_start, host_len);
	parts->host[host_len] = '\0';

	p = host_end;

	// Optional port
	if (*p == ':') {
		p++;
		parts->port = atoi(p);
		if (parts->port <= 0 || parts->port > 65535) {
			return HTTP_ERR_INVALID_URL;
		}
		while (*p >= '0' && *p <= '9') p++;
	}

	// Path (including query string)
	if (*p == '/' || *p == '?') {
		size_t path_len = strlen(p);
		if (path_len >= sizeof(parts->path)) {
			return HTTP_ERR_INVALID_URL;
		}
		if (*p == '?') {
			parts->path[0] = '/';
			strcpy(parts->path + 1, p);
		} else {
			strcpy(parts->path, p);
		}
	}

	return HTTP_OK;
}

/** Create new request */
qd_exec_result usr_http_new(qd_context* ctx) {
	// Pop URL
	qd_stack_element_t url_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &url_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in http::new: stack underflow\n");
		abort();
	}
	if (url_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::new: expected string for url\n");
		abort();
	}

	const char* url = qd_string_data(url_elem.value.s);

	// Allocate request
	http_request_t* req = malloc(sizeof(http_request_t));
	if (!req) {
		qd_string_release(url_elem.value.s);
		fprintf(stderr, "Fatal error in http::new: memory allocation failed\n");
		abort();
	}

	req->url = strdup(url);
	req->method = strdup("GET");
	req->headers_cap = 1024;
	req->headers = malloc(req->headers_cap);
	req->headers[0] = '\0';
	req->headers_len = 0;
	req->body = NULL;
	req->client_cert = NULL;
	req->client_key = NULL;

	qd_string_release(url_elem.value.s);

	qd_push_p(ctx, req);
	return (qd_exec_result){0};
}

/** Set request method */
qd_exec_result usr_http_method(qd_context* ctx) {
	// Pop method
	qd_stack_element_t method_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &method_elem);
	if (err != QD_STACK_OK || method_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::method: expected string\n");
		abort();
	}

	// Pop request
	qd_stack_element_t req_elem;
	err = qd_stack_pop(ctx->st, &req_elem);
	if (err != QD_STACK_OK || req_elem.type != QD_STACK_TYPE_PTR) {
		qd_string_release(method_elem.value.s);
		fprintf(stderr, "Fatal error in http::method: expected request pointer\n");
		abort();
	}

	http_request_t* req = (http_request_t*)req_elem.value.p;
	free(req->method);
	req->method = strdup(qd_string_data(method_elem.value.s));

	qd_string_release(method_elem.value.s);
	return (qd_exec_result){0};
}

/** Add header */
qd_exec_result usr_http_header(qd_context* ctx) {
	// Pop value
	qd_stack_element_t value_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &value_elem);
	if (err != QD_STACK_OK || value_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::header: expected string for value\n");
		abort();
	}

	// Pop name
	qd_stack_element_t name_elem;
	err = qd_stack_pop(ctx->st, &name_elem);
	if (err != QD_STACK_OK || name_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(value_elem.value.s);
		fprintf(stderr, "Fatal error in http::header: expected string for name\n");
		abort();
	}

	// Pop request
	qd_stack_element_t req_elem;
	err = qd_stack_pop(ctx->st, &req_elem);
	if (err != QD_STACK_OK || req_elem.type != QD_STACK_TYPE_PTR) {
		qd_string_release(value_elem.value.s);
		qd_string_release(name_elem.value.s);
		fprintf(stderr, "Fatal error in http::header: expected request pointer\n");
		abort();
	}

	http_request_t* req = (http_request_t*)req_elem.value.p;
	const char* name = qd_string_data(name_elem.value.s);
	const char* value = qd_string_data(value_elem.value.s);

	// Format: "Name: Value\r\n"
	size_t needed = strlen(name) + 2 + strlen(value) + 2 + 1;
	if (req->headers_len + needed > req->headers_cap) {
		req->headers_cap = (req->headers_len + needed) * 2;
		req->headers = realloc(req->headers, req->headers_cap);
	}

	sprintf(req->headers + req->headers_len, "%s: %s\r\n", name, value);
	req->headers_len += needed - 1;

	qd_string_release(name_elem.value.s);
	qd_string_release(value_elem.value.s);
	return (qd_exec_result){0};
}

/** Set body */
qd_exec_result usr_http_body(qd_context* ctx) {
	// Pop body
	qd_stack_element_t body_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &body_elem);
	if (err != QD_STACK_OK || body_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::body: expected string\n");
		abort();
	}

	// Pop request
	qd_stack_element_t req_elem;
	err = qd_stack_pop(ctx->st, &req_elem);
	if (err != QD_STACK_OK || req_elem.type != QD_STACK_TYPE_PTR) {
		qd_string_release(body_elem.value.s);
		fprintf(stderr, "Fatal error in http::body: expected request pointer\n");
		abort();
	}

	http_request_t* req = (http_request_t*)req_elem.value.p;
	free(req->body);
	req->body = strdup(qd_string_data(body_elem.value.s));

	qd_string_release(body_elem.value.s);
	return (qd_exec_result){0};
}

/** Set client certificate for mTLS */
qd_exec_result usr_http_cert(qd_context* ctx) {
	// Pop key_path
	qd_stack_element_t key_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &key_elem);
	if (err != QD_STACK_OK || key_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::cert: expected string for key_path\n");
		abort();
	}

	// Pop cert_path
	qd_stack_element_t cert_elem;
	err = qd_stack_pop(ctx->st, &cert_elem);
	if (err != QD_STACK_OK || cert_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(key_elem.value.s);
		fprintf(stderr, "Fatal error in http::cert: expected string for cert_path\n");
		abort();
	}

	// Pop request
	qd_stack_element_t req_elem;
	err = qd_stack_pop(ctx->st, &req_elem);
	if (err != QD_STACK_OK || req_elem.type != QD_STACK_TYPE_PTR) {
		qd_string_release(key_elem.value.s);
		qd_string_release(cert_elem.value.s);
		fprintf(stderr, "Fatal error in http::cert: expected request pointer\n");
		abort();
	}

	http_request_t* req = (http_request_t*)req_elem.value.p;

	// Free existing paths if any
	free(req->client_cert);
	free(req->client_key);

	req->client_cert = strdup(qd_string_data(cert_elem.value.s));
	req->client_key = strdup(qd_string_data(key_elem.value.s));

	qd_string_release(cert_elem.value.s);
	qd_string_release(key_elem.value.s);
	return (qd_exec_result){0};
}

/** Internal helper to free request struct */
static void free_request_internal(http_request_t* req) {
	if (req) {
		free(req->url);
		free(req->method);
		free(req->headers);
		free(req->body);
		free(req->client_cert);
		free(req->client_key);
		free(req);
	}
}

/** Free request */
qd_exec_result usr_http_free_request(qd_context* ctx) {
	qd_stack_element_t req_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &req_elem);
	if (err != QD_STACK_OK || req_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in http::free_request: expected pointer\n");
		abort();
	}

	free_request_internal((http_request_t*)req_elem.value.p);
	return (qd_exec_result){0};
}

// Forward declarations for net/tls functions
extern qd_exec_result usr_net_connect(qd_context* ctx);
extern qd_exec_result usr_net_send(qd_context* ctx);
extern qd_exec_result usr_net_receive(qd_context* ctx);
extern qd_exec_result usr_net_close(qd_context* ctx);
extern qd_exec_result usr_tls_connect(qd_context* ctx);
extern qd_exec_result usr_tls_connect_mtls(qd_context* ctx);
extern qd_exec_result usr_tls_send(qd_context* ctx);
extern qd_exec_result usr_tls_receive(qd_context* ctx);
extern qd_exec_result usr_tls_close(qd_context* ctx);

/** Parse HTTP response, returns status code or -1 on error */
static int parse_response(const char* data, size_t len, char** headers_out, char** body_out) {
	// Find "HTTP/1.x NNN"
	if (len < 12 || strncmp(data, "HTTP/1.", 7) != 0) {
		return -1;
	}

	// Skip "HTTP/1.x "
	const char* p = data + 9;
	int status = atoi(p);
	if (status < 100 || status > 599) {
		return -1;
	}

	// Find end of headers (\r\n\r\n)
	const char* header_end = strstr(data, "\r\n\r\n");
	if (!header_end) {
		// No body, headers only
		*headers_out = strdup(data);
		*body_out = strdup("");
		return status;
	}

	// Extract headers (skip status line)
	const char* first_header = strchr(data, '\n');
	if (first_header && first_header < header_end) {
		first_header++;
		size_t headers_len = (size_t)(header_end - first_header);
		*headers_out = malloc(headers_len + 1);
		memcpy(*headers_out, first_header, headers_len);
		(*headers_out)[headers_len] = '\0';
	} else {
		*headers_out = strdup("");
	}

	// Extract body
	const char* body_start = header_end + 4;
	size_t body_len = len - (size_t)(body_start - data);
	*body_out = malloc(body_len + 1);
	memcpy(*body_out, body_start, body_len);
	(*body_out)[body_len] = '\0';

	return status;
}

/** Execute HTTP request */
qd_exec_result usr_http_send(qd_context* ctx) {
	// Pop request
	qd_stack_element_t req_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &req_elem);
	if (err != QD_STACK_OK || req_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in http::send: expected request pointer\n");
		abort();
	}

	http_request_t* req = (http_request_t*)req_elem.value.p;

	// Parse URL
	url_parts_t url;
	int parse_err = parse_url(req->url, &url);
	if (parse_err != HTTP_OK) {
		ctx->error_code = HTTP_ERR_INVALID_URL;
		qd_push_i(ctx, HTTP_ERR_INVALID_URL);
		return (qd_exec_result){HTTP_ERR_INVALID_URL};
	}

	// Connect via net module
	qd_push_s(ctx, url.host);
	qd_push_i(ctx, url.port);
	qd_exec_result connect_result = usr_net_connect(ctx);

	// Pop socket fd
	qd_stack_element_t sock_elem;
	err = qd_stack_pop(ctx->st, &sock_elem);
	if (err != QD_STACK_OK || connect_result.code != 0) {
		ctx->error_code = HTTP_ERR_CONNECT;
		qd_push_i(ctx, HTTP_ERR_CONNECT);
		return (qd_exec_result){HTTP_ERR_CONNECT};
	}
	int sock_fd = (int)sock_elem.value.i;

	// For HTTPS, wrap with TLS (use mTLS if client cert is set)
	void* tls_conn = NULL;
	if (url.is_https) {
		qd_push_i(ctx, sock_fd);
		qd_push_s(ctx, url.host);

		if (req->client_cert && req->client_key) {
			// Use mTLS with client certificate
			qd_push_s(ctx, req->client_cert);
			qd_push_s(ctx, req->client_key);
			(void)usr_tls_connect_mtls(ctx);
		} else {
			// Standard TLS
			(void)usr_tls_connect(ctx);
		}

		// Pop status code
		qd_stack_element_t tls_status;
		qd_stack_pop(ctx->st, &tls_status);

		if (tls_status.value.i != 1) { // TLS_OK = 1
			// Close socket
			qd_push_i(ctx, sock_fd);
			usr_net_close(ctx);
			ctx->error_code = HTTP_ERR_TLS;
			qd_push_i(ctx, HTTP_ERR_TLS);
			return (qd_exec_result){HTTP_ERR_TLS};
		}

		// Pop TLS connection
		qd_stack_element_t conn_elem;
		qd_stack_pop(ctx->st, &conn_elem);
		tls_conn = conn_elem.value.p;
	}

	// Build HTTP request
	size_t body_len = req->body ? strlen(req->body) : 0;
	size_t request_size = strlen(req->method) + 1 + strlen(url.path) + 32 +
	                      strlen(url.host) + 32 + req->headers_len + body_len + 128;
	char* request = malloc(request_size);

	int written = sprintf(request, "%s %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n",
	                      req->method, url.path, url.host);

	if (req->headers_len > 0) {
		written += sprintf(request + written, "%s", req->headers);
	}

	if (body_len > 0) {
		written += sprintf(request + written, "Content-Length: %zu\r\n\r\n%s",
		                   body_len, req->body);
	} else {
		written += sprintf(request + written, "\r\n");
	}
	(void)written; // Silence unused warning

	// Send request
	if (url.is_https) {
		qd_push_p(ctx, tls_conn);
		qd_push_s(ctx, request);
		(void)usr_tls_send(ctx);

		// Pop status
		qd_stack_element_t send_status;
		qd_stack_pop(ctx->st, &send_status);
		if (send_status.value.i != 1) {
			// Pop bytes_sent if present
			qd_stack_element_t dummy;
			qd_stack_pop(ctx->st, &dummy);
			free(request);
			qd_push_p(ctx, tls_conn);
			usr_tls_close(ctx);
			qd_push_i(ctx, sock_fd);
			usr_net_close(ctx);
			ctx->error_code = HTTP_ERR_SEND;
			qd_push_i(ctx, HTTP_ERR_SEND);
			return (qd_exec_result){HTTP_ERR_SEND};
		}
		// Pop bytes_sent
		qd_stack_element_t bytes_sent;
		qd_stack_pop(ctx->st, &bytes_sent);
	} else {
		qd_push_i(ctx, sock_fd);
		qd_push_s(ctx, request);
		(void)usr_net_send(ctx);
		// Pop bytes_sent
		qd_stack_element_t bytes_sent;
		qd_stack_pop(ctx->st, &bytes_sent);
	}
	free(request);

	// Receive response
	char* response_data = malloc(65536);
	size_t response_len = 0;
	size_t response_cap = 65536;

	while (1) {
		if (url.is_https) {
			qd_push_p(ctx, tls_conn);
			qd_push_i(ctx, 8192);
			usr_tls_receive(ctx);

			// Pop status
			qd_stack_element_t recv_status;
			qd_stack_pop(ctx->st, &recv_status);

			if (recv_status.value.i != 1) {
				// Connection closed or error - check if we have data
				break;
			}

			// Pop bytes_read
			qd_stack_element_t bytes_read;
			qd_stack_pop(ctx->st, &bytes_read);

			// Pop data
			qd_stack_element_t data_elem;
			qd_stack_pop(ctx->st, &data_elem);

			if (bytes_read.value.i <= 0) {
				qd_string_release(data_elem.value.s);
				break;
			}

			const char* chunk = qd_string_data(data_elem.value.s);
			size_t chunk_len = (size_t)bytes_read.value.i;

			if (response_len + chunk_len >= response_cap) {
				response_cap = (response_len + chunk_len) * 2;
				response_data = realloc(response_data, response_cap);
			}
			memcpy(response_data + response_len, chunk, chunk_len);
			response_len += chunk_len;
			qd_string_release(data_elem.value.s);
		} else {
			qd_push_i(ctx, sock_fd);
			qd_push_i(ctx, 8192);
			usr_net_receive(ctx);

			// Pop bytes_read
			qd_stack_element_t bytes_read;
			qd_stack_pop(ctx->st, &bytes_read);

			// Pop data
			qd_stack_element_t data_elem;
			qd_stack_pop(ctx->st, &data_elem);

			if (bytes_read.value.i <= 0) {
				qd_string_release(data_elem.value.s);
				break;
			}

			const char* chunk = qd_string_data(data_elem.value.s);
			size_t chunk_len = (size_t)bytes_read.value.i;

			if (response_len + chunk_len >= response_cap) {
				response_cap = (response_len + chunk_len) * 2;
				response_data = realloc(response_data, response_cap);
			}
			memcpy(response_data + response_len, chunk, chunk_len);
			response_len += chunk_len;
			qd_string_release(data_elem.value.s);
		}
	}
	response_data[response_len] = '\0';

	// Close connections
	if (url.is_https) {
		qd_push_p(ctx, tls_conn);
		usr_tls_close(ctx);
	}
	qd_push_i(ctx, sock_fd);
	usr_net_close(ctx);

	// Parse response
	char* headers = NULL;
	char* body = NULL;
	int status = parse_response(response_data, response_len, &headers, &body);
	free(response_data);

	if (status < 0) {
		free(headers);
		free(body);
		ctx->error_code = HTTP_ERR_PARSE;
		qd_push_i(ctx, HTTP_ERR_PARSE);
		return (qd_exec_result){HTTP_ERR_PARSE};
	}

	// Create response struct
	http_response_t* resp = malloc(sizeof(http_response_t));
	resp->status = status;
	resp->headers = qd_string_create(headers);
	resp->body = qd_string_create(body);
	free(headers);
	free(body);

	qd_push_p(ctx, resp);
	qd_push_i(ctx, HTTP_OK);
	return (qd_exec_result){0};
}

/** Close response */
qd_exec_result usr_http_close(qd_context* ctx) {
	qd_stack_element_t resp_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &resp_elem);
	if (err != QD_STACK_OK || resp_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in http::close: expected pointer\n");
		abort();
	}

	http_response_t* resp = (http_response_t*)resp_elem.value.p;
	if (resp) {
		if (resp->headers) qd_string_release(resp->headers);
		if (resp->body) qd_string_release(resp->body);
		free(resp);
	}
	return (qd_exec_result){0};
}

/** Simple GET request */
qd_exec_result usr_http_get(qd_context* ctx) {
	// URL is already on stack
	// Create request
	usr_http_new(ctx);

	// Get request pointer (it's on stack but we need to free it on error)
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	http_request_t* req = (http_request_t*)req_elem.value.p;

	// Push it back for send
	qd_push_p(ctx, req);

	// Send it
	qd_exec_result result = usr_http_send(ctx);

	// Free request regardless of success or failure
	free_request_internal(req);

	return result;
}
