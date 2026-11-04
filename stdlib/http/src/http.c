#define _DEFAULT_SOURCE
/**
 * @file http.c
 * @brief HTTP client implementation for Quadrate
 */

#define _POSIX_C_SOURCE 200809L

#include "http_internal.h"
#include <errno.h>
#include <netdb.h>
#include <platform/tls_platform.h>
#include <quadrate/http/http.h>
#include <quadrate/rt/runtime.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

bool http_buf_add(http_buf_t* b, const char* s, size_t n) {
	if (n >= SIZE_MAX - b->len) {
		return false;
	}
	const size_t need = b->len + n + 1;
	if (need > b->cap) {
		size_t cap = b->cap ? b->cap : 256;
		while (cap < need) {
			cap = cap > SIZE_MAX / 2 ? need : cap * 2;
		}
		char* data = realloc(b->data, cap);
		if (!data) {
			return false;
		}
		b->data = data;
		b->cap = cap;
	}
	memcpy(b->data + b->len, s, n);
	b->len += n;
	b->data[b->len] = '\0';
	return true;
}

bool http_buf_printf(http_buf_t* b, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	const int n = vsnprintf(NULL, 0, fmt, args);
	va_end(args);
	if (n < 0) {
		return false;
	}
	char* text = malloc((size_t)n + 1);
	if (!text) {
		return false;
	}
	va_start(args, fmt);
	vsnprintf(text, (size_t)n + 1, fmt, args);
	va_end(args);
	const bool ok = http_buf_add(b, text, (size_t)n);
	free(text);
	return ok;
}

/** HTTP Request structure */
typedef struct {
	char* url;
	char* method;
	http_buf_t headers; // "Name: value\r\n" lines
	char* body;			// NULL when no body was set
	char* client_cert;	// Path to client certificate for mTLS
	char* client_key;	// Path to client private key for mTLS
} http_request_t;

/** HTTP Response structure, laid out as the Quadrate struct `Response`: three 8-byte fields */
typedef struct {
	int64_t status;
	qd_string_t* headers;
	qd_string_t* body;
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
	parts->path[0] = '/';
	parts->path[1] = '\0';

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
		char* endptr;
		long port_val = strtol(p, &endptr, 10);
		if (endptr == p || port_val <= 0 || port_val > 65535) {
			return HTTP_ERR_INVALID_URL;
		}
		parts->port = (int)port_val;
		p = endptr;
	}

	// Path (including query string)
	if (*p == '/' || *p == '?') {
		size_t path_len = strlen(p);
		if (*p == '?') {
			// Need room for '/' + path + null
			if (path_len + 1 >= sizeof(parts->path)) {
				return HTTP_ERR_INVALID_URL;
			}
			parts->path[0] = '/';
			memcpy(parts->path + 1, p, path_len + 1);
		} else {
			if (path_len >= sizeof(parts->path)) {
				return HTTP_ERR_INVALID_URL;
			}
			memcpy(parts->path, p, path_len + 1);
		}
	} else if (*p != '\0') {
		return HTTP_ERR_INVALID_URL;
	}

	return HTTP_OK;
}

static char* dup_or_raise(qd_context* ctx, const char* op, const char* s) {
	char* copy = strdup(s);
	if (!copy) {
		qd_fatal_raise(ctx, op, "Out of memory");
	}
	return copy;
}

// A line break in a method or header would end the line early and let the rest of the
// value be read as further headers, or as a second request
static void reject_line_break(qd_context* ctx, const char* op, const char* what, qd_string_t* s) {
	if (strpbrk(qd_string_data(s), "\r\n") != NULL) {
		qd_string_release(s);
		qd_fatal_raise(ctx, op, "%s contains a line break", what);
	}
}

static http_request_t* request_new(qd_context* ctx, const char* op, const char* url) {
	http_request_t* req = calloc(1, sizeof(http_request_t));
	if (!req) {
		qd_fatal_raise(ctx, op, "Out of memory");
	}
	req->url = dup_or_raise(ctx, op, url);
	req->method = dup_or_raise(ctx, op, "GET");
	return req;
}

static void request_free(http_request_t* req) {
	if (req) {
		free(req->url);
		free(req->method);
		free(req->headers.data);
		free(req->body);
		free(req->client_cert);
		free(req->client_key);
		free(req);
	}
}

// Stack signature: ( url:str -- req:ptr )
int usr_http_new(qd_context* ctx) {
	qd_string_t* url;
	qd_args(ctx, "http::new", "s", &url);
	http_request_t* req = request_new(ctx, "http::new", qd_string_data(url));
	qd_string_release(url);
	return qd_push_p(ctx, req);
}

// Stack signature: ( req:ptr method:str -- )
int usr_http_method(qd_context* ctx) {
	void* handle;
	qd_string_t* method;
	qd_args(ctx, "http::method", "ps", &handle, &method);
	reject_line_break(ctx, "http::method", "the method", method);
	http_request_t* req = (http_request_t*)handle;
	free(req->method);
	req->method = strdup(qd_string_data(method));
	qd_string_release(method);
	if (!req->method) {
		qd_fatal_raise(ctx, "http::method", "Out of memory");
	}
	return 0;
}

// Stack signature: ( req:ptr name:str value:str -- )
int usr_http_header(qd_context* ctx) {
	void* handle;
	qd_string_t* name;
	qd_string_t* value;
	qd_args(ctx, "http::header", "pss", &handle, &name, &value);
	if (strpbrk(qd_string_data(name), "\r\n:") != NULL || strpbrk(qd_string_data(value), "\r\n") != NULL) {
		qd_string_release(name);
		qd_string_release(value);
		qd_fatal_raise(ctx, "http::header", "a header name or value contains a line break, or the name a colon");
	}
	http_request_t* req = (http_request_t*)handle;
	const bool ok = http_buf_printf(&req->headers, "%s: %s\r\n", qd_string_data(name), qd_string_data(value));
	qd_string_release(name);
	qd_string_release(value);
	if (!ok) {
		qd_fatal_raise(ctx, "http::header", "Out of memory");
	}
	return 0;
}

// Stack signature: ( req:ptr body:str -- )
int usr_http_body(qd_context* ctx) {
	void* handle;
	qd_string_t* body;
	qd_args(ctx, "http::body", "ps", &handle, &body);
	http_request_t* req = (http_request_t*)handle;
	free(req->body);
	req->body = strdup(qd_string_data(body));
	qd_string_release(body);
	if (!req->body) {
		qd_fatal_raise(ctx, "http::body", "Out of memory");
	}
	return 0;
}

// Stack signature: ( req:ptr cert_path:str key_path:str -- )
int usr_http_cert(qd_context* ctx) {
	void* handle;
	qd_string_t* cert;
	qd_string_t* key;
	qd_args(ctx, "http::cert", "pss", &handle, &cert, &key);
	http_request_t* req = (http_request_t*)handle;
	free(req->client_cert);
	free(req->client_key);
	req->client_cert = strdup(qd_string_data(cert));
	req->client_key = strdup(qd_string_data(key));
	qd_string_release(cert);
	qd_string_release(key);
	if (!req->client_cert || !req->client_key) {
		qd_fatal_raise(ctx, "http::cert", "Out of memory");
	}
	return 0;
}

// Stack signature: ( req:ptr -- )
int usr_http_free_request(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "http::free_request", "p", &handle);
	request_free((http_request_t*)handle);
	return 0;
}

/** An open connection: a socket, and the TLS session over it for https */
typedef struct {
	int fd;
	tls_conn_t tls;
} conn_t;

static int tcp_connect(const char* host, int port) {
	char service[8];
	snprintf(service, sizeof(service), "%d", port);
	struct addrinfo hints = {0};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	struct addrinfo* found = NULL;
	if (getaddrinfo(host, service, &hints, &found) != 0) {
		return -1;
	}
	int fd = -1;
	for (struct addrinfo* a = found; a != NULL && fd < 0; a = a->ai_next) {
		fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
		if (fd >= 0 && connect(fd, a->ai_addr, a->ai_addrlen) != 0) {
			close(fd);
			fd = -1;
		}
	}
	freeaddrinfo(found);
	return fd;
}

// One write may take only part of the data
static bool conn_write_all(conn_t* c, const char* data, size_t len) {
	size_t off = 0;
	while (off < len) {
		if (c->tls != TLS_CONN_INVALID) {
			int sent = 0;
			if (tls_platform_send(c->tls, data + off, len - off, &sent) != TLS_OK || sent <= 0) {
				return false;
			}
			off += (size_t)sent;
		} else {
			const ssize_t sent = write(c->fd, data + off, len - off);
			if (sent < 0 && errno == EINTR) {
				continue;
			}
			if (sent <= 0) {
				return false;
			}
			off += (size_t)sent;
		}
	}
	return true;
}

// Bytes read, 0 when the peer closed the connection, -1 on an error
static ssize_t conn_read(conn_t* c, char* buf, size_t cap) {
	if (c->tls != TLS_CONN_INVALID) {
		int got = 0;
		const tls_error_t err = tls_platform_receive(c->tls, buf, cap, &got);
		if (err == TLS_OK) {
			return got;
		}
		return err == TLS_ERR_CLOSED ? 0 : -1;
	}
	for (;;) {
		const ssize_t got = read(c->fd, buf, cap);
		if (got >= 0 || errno != EINTR) {
			return got;
		}
	}
}

static void conn_close(conn_t* c) {
	if (c->tls != TLS_CONN_INVALID) {
		tls_platform_close(c->tls);
	}
	close(c->fd);
}

static bool build_request(http_buf_t* out, const http_request_t* req, const url_parts_t* url) {
	const int default_port = url->is_https ? 443 : 80;
	bool ok = http_buf_printf(out, "%s %s HTTP/1.1\r\nHost: %s", req->method, url->path, url->host);
	if (ok && url->port != default_port) {
		ok = http_buf_printf(out, ":%d", url->port);
	}
	ok = ok && http_buf_printf(out, "\r\nConnection: close\r\n");
	if (ok && req->headers.len > 0) {
		ok = http_buf_add(out, req->headers.data, req->headers.len);
	}
	if (ok && req->body) {
		ok = http_buf_printf(out, "Content-Length: %zu\r\n\r\n", strlen(req->body)) &&
			 http_buf_add(out, req->body, strlen(req->body));
	} else if (ok) {
		ok = http_buf_add(out, "\r\n", 2);
	}
	return ok;
}

const char* http_find_header(const char* headers, size_t len, const char* name, size_t* value_len) {
	const size_t name_len = strlen(name);
	const char* end = headers + len;
	for (const char* line = headers; line < end;) {
		const char* line_end = memmem(line, (size_t)(end - line), "\r\n", 2);
		if (!line_end) {
			line_end = end;
		}
		if ((size_t)(line_end - line) > name_len && strncasecmp(line, name, name_len) == 0 && line[name_len] == ':') {
			const char* v = line + name_len + 1;
			const char* v_end = line_end;
			while (v < v_end && (*v == ' ' || *v == '\t')) {
				v++;
			}
			while (v_end > v && (v_end[-1] == ' ' || v_end[-1] == '\t')) {
				v_end--;
			}
			*value_len = (size_t)(v_end - v);
			return v;
		}
		line = line_end + 2;
	}
	return NULL;
}

// Decode a chunked body in place. Its length, or -1 when it is malformed or cut short.
static ssize_t dechunk(char* body, size_t len) {
	size_t in = 0;
	size_t out = 0;
	for (;;) {
		const char* line_end = memmem(body + in, len - in, "\r\n", 2);
		if (!line_end) {
			return -1;
		}
		char* digits_end;
		const unsigned long long size = strtoull(body + in, &digits_end, 16);
		if (digits_end == body + in) {
			return -1;
		}
		in = (size_t)(line_end - body) + 2;
		if (size == 0) {
			return (ssize_t)out;
		}
		if (size > len - in || len - in - size < 2 || memcmp(body + in + size, "\r\n", 2) != 0) {
			return -1;
		}
		memmove(body + out, body + in, size);
		out += size;
		in += size + 2;
	}
}

// Read the response from `data`. `complete` says the peer closed the connection cleanly;
// otherwise the read failed, and the response counts only if its own framing says it is
// all there.
static int parse_response(char* data, size_t len, bool complete, http_response_t** out) {
	*out = NULL;
	if (len < 12 || strncmp(data, "HTTP/1.", 7) != 0) {
		return complete ? HTTP_ERR_PARSE : HTTP_ERR_RECEIVE;
	}
	char* endptr;
	const long status = strtol(data + 9, &endptr, 10);
	const char* header_end = memmem(data, len, "\r\n\r\n", 4);
	if (endptr == data + 9 || status < 100 || status > 599 || !header_end) {
		return complete ? HTTP_ERR_PARSE : HTTP_ERR_RECEIVE;
	}

	// The header lines, without the status line or the blank line that ends them
	const char* first_header = memchr(data, '\n', (size_t)(header_end - data));
	first_header = first_header ? first_header + 1 : header_end;
	const size_t headers_len = (size_t)(header_end - first_header);

	char* body = (char*)header_end + 4;
	size_t body_len = len - (size_t)(body - data);

	size_t te_len;
	size_t cl_len;
	const char* te = http_find_header(first_header, headers_len, "Transfer-Encoding", &te_len);
	const char* cl = http_find_header(first_header, headers_len, "Content-Length", &cl_len);
	if (te && te_len >= 7 && strncasecmp(te + te_len - 7, "chunked", 7) == 0) {
		const ssize_t decoded = dechunk(body, body_len);
		if (decoded < 0) {
			return complete ? HTTP_ERR_PARSE : HTTP_ERR_RECEIVE;
		}
		body_len = (size_t)decoded;
	} else if (cl) {
		const unsigned long long want = strtoull(cl, NULL, 10);
		if (body_len > want) {
			body_len = (size_t)want;
		} else if (body_len < want && !complete) {
			return HTTP_ERR_RECEIVE;
		}
	} else if (!complete) {
		return HTTP_ERR_RECEIVE;
	}

	http_response_t* resp = malloc(sizeof(http_response_t));
	if (!resp) {
		return HTTP_ERR_MEMORY;
	}
	resp->status = status;
	resp->headers = qd_string_create_with_length(first_header, headers_len);
	resp->body = qd_string_create_with_length(body, body_len);
	*out = resp;
	return HTTP_OK;
}

static int send_request(qd_context* ctx, const char* op, const http_request_t* req) {
	url_parts_t url;
	if (parse_url(req->url, &url) != HTTP_OK) {
		return qd_fail(ctx, HTTP_ERR_INVALID_URL, "%s: invalid URL: %s", op, req->url);
	}

	http_buf_t request = {0};
	if (!build_request(&request, req, &url)) {
		free(request.data);
		return qd_fail(ctx, HTTP_ERR_MEMORY, "%s: out of memory", op);
	}

	conn_t conn = {tcp_connect(url.host, url.port), TLS_CONN_INVALID};
	if (conn.fd < 0) {
		free(request.data);
		return qd_fail(ctx, HTTP_ERR_CONNECT, "%s: could not connect to %s:%d", op, url.host, url.port);
	}
	if (url.is_https) {
		const tls_error_t err =
				(req->client_cert && req->client_key)
						? tls_platform_connect_mtls(conn.fd, url.host, req->client_cert, req->client_key, &conn.tls)
						: tls_platform_connect(conn.fd, url.host, &conn.tls);
		if (err != TLS_OK) {
			free(request.data);
			close(conn.fd);
			return qd_fail(ctx, HTTP_ERR_TLS, "%s: TLS with %s failed: %s: %s", op, url.host,
					tls_platform_error_string(err), tls_platform_error_detail());
		}
	}

	const bool sent = conn_write_all(&conn, request.data, request.len);
	free(request.data);
	if (!sent) {
		conn_close(&conn);
		return qd_fail(ctx, HTTP_ERR_SEND, "%s: could not send the request to %s", op, url.host);
	}

	http_buf_t response = {0};
	char chunk[8192];
	ssize_t got;
	while ((got = conn_read(&conn, chunk, sizeof(chunk))) > 0) {
		if (!http_buf_add(&response, chunk, (size_t)got)) {
			conn_close(&conn);
			free(response.data);
			return qd_fail(ctx, HTTP_ERR_MEMORY, "%s: out of memory", op);
		}
	}
	conn_close(&conn);

	char nothing[1] = "";
	http_response_t* resp;
	const int status = parse_response(response.data ? response.data : nothing, response.len, got == 0, &resp);
	free(response.data);
	switch (status) {
	case HTTP_OK:
		qd_push_p(ctx, resp);
		qd_push_i(ctx, HTTP_OK);
		return 0;
	case HTTP_ERR_RECEIVE:
		return qd_fail(ctx, status, "%s: the connection to %s failed before the response was complete", op, url.host);
	case HTTP_ERR_PARSE:
		return qd_fail(ctx, status, "%s: %s did not send a valid HTTP response", op, url.host);
	default:
		return qd_fail(ctx, status, "%s: out of memory", op);
	}
}

// Stack signature: ( req:ptr -- resp:Response )!
int usr_http_send(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "http::send", "p", &handle);
	return send_request(ctx, "http::send", (http_request_t*)handle);
}

// Stack signature: ( url:str -- resp:Response )!
int usr_http_get(qd_context* ctx) {
	qd_string_t* url;
	qd_args(ctx, "http::get", "s", &url);
	http_request_t* req = request_new(ctx, "http::get", qd_string_data(url));
	qd_string_release(url);
	const int result = send_request(ctx, "http::get", req);
	request_free(req);
	return result;
}

// Stack signature: ( resp:Response -- )
int usr_http_close(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "http::close", "p", &handle);
	http_response_t* resp = (http_response_t*)handle;
	if (resp) {
		if (resp->headers) {
			qd_string_release(resp->headers);
		}
		if (resp->body) {
			qd_string_release(resp->body);
		}
		free(resp);
	}
	return 0;
}
