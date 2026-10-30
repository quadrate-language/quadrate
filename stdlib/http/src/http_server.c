#define _DEFAULT_SOURCE
/**
 * @file http_server.c
 * @brief HTTP server implementation for Quadrate (Gin-inspired API)
 */

#define _POSIX_C_SOURCE 200809L

#include "http_internal.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <quadrate/http/http.h>
#include <quadrate/rt/runtime.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <threads.h>
#include <unistd.h>

// ============================================================
// Internal Structures
// ============================================================

/** HTTP Method enum */
typedef enum {
	HTTP_METHOD_GET,
	HTTP_METHOD_POST,
	HTTP_METHOD_PUT,
	HTTP_METHOD_DELETE,
	HTTP_METHOD_ANY
} http_method_t;

/** Route definition */
typedef struct {
	http_method_t method;
	char* path;	   // Pattern like "/users/:id"
	void* handler; // Function pointer
	int group_idx; // -1 for root, otherwise group index
} http_route_t;

/** Route group */
typedef struct {
	char* prefix;
	void* middlewares[HTTP_MAX_MIDDLEWARE];
	int middleware_count;
} http_group_t;

/** HTTP Engine */
typedef struct {
	int server_fd;
	// `http::stop` runs in a handler's thread while `http::run` waits in accept
	atomic_bool running;

	// Routes
	http_route_t routes[HTTP_MAX_ROUTES];
	int route_count;

	// Global middleware
	void* middlewares[HTTP_MAX_MIDDLEWARE];
	int middleware_count;

	// Groups
	http_group_t groups[HTTP_MAX_GROUPS];
	int group_count;

	// Group handles (for cleanup)
	void* group_handles[HTTP_MAX_GROUPS];
	int group_handle_count;
} http_engine_t;

/** HTTP Context (matches Quadrate Ctx struct layout) */
typedef struct {
	qd_string_t* method;
	qd_string_t* path;
	qd_string_t* query;
	qd_string_t* headers;
	qd_string_t* body;
	qd_string_t* params;
	int64_t socket;
	int64_t responded;
	// Internal, not exposed to Quadrate: the headers `http::set_header` added
	http_buf_t response_headers;
	// Internal: SSE mode flag (1 = SSE started, connection kept open)
	int sse_mode;
} http_ctx_t;

/** Thread arguments for request handling */
typedef struct {
	http_engine_t* engine;
	int client_fd;
} http_thread_args_t;

// Forward declaration
static void handle_request(http_engine_t* engine, int client_fd, qd_context* ctx);

// ============================================================
// Helper Functions
// ============================================================

/** Get HTTP status text */
static const char* http_status_text(int status) {
	switch (status) {
	case 200:
		return "OK";
	case 201:
		return "Created";
	case 204:
		return "No Content";
	case 301:
		return "Moved Permanently";
	case 302:
		return "Found";
	case 304:
		return "Not Modified";
	case 400:
		return "Bad Request";
	case 401:
		return "Unauthorized";
	case 403:
		return "Forbidden";
	case 404:
		return "Not Found";
	case 405:
		return "Method Not Allowed";
	case 413:
		return "Content Too Large";
	case 431:
		return "Request Header Fields Too Large";
	case 500:
		return "Internal Server Error";
	case 502:
		return "Bad Gateway";
	case 503:
		return "Service Unavailable";
	default:
		return "Unknown";
	}
}

/** Parse address string like ":8080" or "127.0.0.1:3000" */
static int parse_addr(const char* addr, char* host, size_t host_size, int* port) {
	const char* colon = strchr(addr, ':');
	if (!colon) {
		return -1;
	}

	if (colon == addr) {
		if (host_size < 8) {
			return -1; // "0.0.0.0" + null
		}
		memcpy(host, "0.0.0.0", 8);
	} else {
		size_t len = (size_t)(colon - addr);
		if (len >= host_size) {
			return -1;
		}
		memcpy(host, addr, len);
		host[len] = '\0';
	}

	// Validate port string contains only digits
	const char* port_str = colon + 1;
	if (*port_str == '\0') {
		return -1;
	}
	for (const char* p = port_str; *p; p++) {
		if (*p < '0' || *p > '9') {
			return -1;
		}
	}

	long port_val = strtol(port_str, NULL, 10);
	if (port_val <= 0 || port_val > 65535) {
		return -1;
	}
	*port = (int)port_val;

	return 0;
}

/** Match route pattern against path, extract params */
static bool match_route(const char* pattern, const char* path, char* params_out, size_t params_size) {
	params_out[0] = '\0';
	size_t params_len = 0;

	const char* p = pattern;
	const char* q = path;

	while (*p && *q) {
		// Wildcard: /* matches everything after
		if (*p == '/' && p[1] == '*' && p[2] == '\0') {
			// Store rest of path as "_path" param
			size_t rest_len = strlen(q);
			size_t needed = (size_t)(params_len > 0 ? 1 : 0) + 5 + 1 + rest_len + 1; // "_path=..."
			if (params_len + needed <= params_size) {
				if (params_len > 0) {
					params_out[params_len++] = '\n';
				}
				memcpy(params_out + params_len, "_path", 5);
				params_len += 5;
				params_out[params_len++] = '=';
				memcpy(params_out + params_len, q, rest_len);
				params_len += rest_len;
				params_out[params_len] = '\0';
			}
			return true; // Wildcard matches everything
		}

		if (*p == ':') {
			// Extract parameter name
			p++;
			const char* name_start = p;
			while (*p && *p != '/') {
				p++;
			}
			size_t name_len = (size_t)(p - name_start);

			// Extract value from path
			const char* val_start = q;
			while (*q && *q != '/') {
				q++;
			}
			size_t val_len = (size_t)(q - val_start);

			// Check bounds before adding to params
			size_t needed = (params_len > 0 ? 1 : 0) + name_len + 1 + val_len + 1;
			if (params_len + needed > params_size) {
				return false; // Params buffer too small
			}

			// Add to params
			if (params_len > 0) {
				params_out[params_len++] = '\n';
			}
			memcpy(params_out + params_len, name_start, name_len);
			params_len += name_len;
			params_out[params_len++] = '=';
			memcpy(params_out + params_len, val_start, val_len);
			params_len += val_len;
			params_out[params_len] = '\0';
		} else if (*p == *q) {
			p++;
			q++;
		} else {
			return false;
		}
	}

	// Check for wildcard at end of pattern (when path is exactly the prefix)
	if (*p == '/' && p[1] == '*' && p[2] == '\0') {
		return true;
	}

	// Both should be at end (or pattern could end with trailing slash)
	if (*p == '/' && !p[1]) {
		p++;
	}
	if (*q == '/' && !q[1]) {
		q++;
	}

	return (*p == '\0' && *q == '\0');
}

/** Convert method string to enum */
static http_method_t method_from_string(const char* method) {
	if (strcmp(method, "GET") == 0) {
		return HTTP_METHOD_GET;
	}
	if (strcmp(method, "POST") == 0) {
		return HTTP_METHOD_POST;
	}
	if (strcmp(method, "PUT") == 0) {
		return HTTP_METHOD_PUT;
	}
	if (strcmp(method, "DELETE") == 0) {
		return HTTP_METHOD_DELETE;
	}
	return HTTP_METHOD_ANY;
}

/** Find parameter value */
static const char* find_param(const char* params, const char* name, char* value_out, size_t max_len) {
	size_t name_len = strlen(name);
	const char* p = params;

	while (*p) {
		// Check if this line starts with name=
		if (strncmp(p, name, name_len) == 0 && p[name_len] == '=') {
			const char* val = p + name_len + 1;
			const char* end = strchr(val, '\n');
			size_t val_len = end ? (size_t)(end - val) : strlen(val);
			if (val_len >= max_len) {
				val_len = max_len - 1;
			}
			memcpy(value_out, val, val_len);
			value_out[val_len] = '\0';
			return value_out;
		}

		// Next line
		const char* nl = strchr(p, '\n');
		if (!nl) {
			break;
		}
		p = nl + 1;
	}

	value_out[0] = '\0';
	return NULL;
}

/** Find query parameter */
static const char* find_query_param(const char* query, const char* name, char* value_out, size_t max_len) {
	size_t name_len = strlen(name);
	const char* p = query;

	while (*p) {
		// Check if this segment starts with name=
		if (strncmp(p, name, name_len) == 0 && p[name_len] == '=') {
			const char* val = p + name_len + 1;
			const char* end = strchr(val, '&');
			size_t val_len = end ? (size_t)(end - val) : strlen(val);
			if (val_len >= max_len) {
				val_len = max_len - 1;
			}
			memcpy(value_out, val, val_len);
			value_out[val_len] = '\0';
			return value_out;
		}

		// Next parameter
		const char* amp = strchr(p, '&');
		if (!amp) {
			break;
		}
		p = amp + 1;
	}

	value_out[0] = '\0';
	return NULL;
}

// One send may take only part of the data. MSG_NOSIGNAL: a client that has hung up must
// fail the send, not raise SIGPIPE and end the whole server.
static bool send_all(int socket, const char* data, size_t len) {
	size_t off = 0;
	while (off < len) {
		const ssize_t sent = send(socket, data + off, len - off, MSG_NOSIGNAL);
		if (sent < 0 && errno == EINTR) {
			continue;
		}
		if (sent <= 0) {
			return false;
		}
		off += (size_t)sent;
	}
	return true;
}

// A response's head; `body_len` < 0 sends no Content-Length, for an event stream
static bool send_head(int socket, int status, const char* content_type, long long body_len, const char* extra,
		const http_buf_t* headers) {
	http_buf_t head = {0};
	bool ok = http_buf_printf(
			&head, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\n", status, http_status_text(status), content_type);
	if (ok && body_len >= 0) {
		ok = http_buf_printf(&head, "Content-Length: %lld\r\nConnection: close\r\n", body_len);
	}
	if (ok && extra) {
		ok = http_buf_add(&head, extra, strlen(extra));
	}
	if (ok && headers && headers->len > 0) {
		ok = http_buf_add(&head, headers->data, headers->len);
	}
	ok = ok && http_buf_add(&head, "\r\n", 2) && send_all(socket, head.data, head.len);
	free(head.data);
	return ok;
}

/** Send HTTP response */
static bool send_response(int socket, int status, const http_buf_t* headers, const char* content_type, const char* body,
		size_t body_len) {
	return send_head(socket, status, content_type, (long long)body_len, NULL, headers) &&
		   send_all(socket, body, body_len);
}

static bool send_text(int socket, int status, const http_buf_t* headers, const char* body) {
	return send_response(socket, status, headers, "text/plain", body, strlen(body));
}

// A line break in a response header would let its value write further headers
static void reject_line_break(qd_context* ctx, const char* op, qd_string_t* name, qd_string_t* value) {
	if (strpbrk(qd_string_data(name), "\r\n:") != NULL || strpbrk(qd_string_data(value), "\r\n") != NULL) {
		qd_string_release(name);
		qd_string_release(value);
		qd_fatal_raise(ctx, op, "a header name or value contains a line break, or the name a colon");
	}
}

// ============================================================
// Engine API
// ============================================================

int usr_http_engine(qd_context* ctx) {
	http_engine_t* engine = calloc(1, sizeof(http_engine_t));
	if (!engine) {
		qd_fatal_raise(ctx, "http::engine", "Out of memory");
	}
	engine->server_fd = -1;
	atomic_init(&engine->running, false);
	return qd_push_p(ctx, engine);
}

static void add_route(qd_context* ctx, const char* op, http_engine_t* engine, http_method_t method, qd_string_t* path,
		void* handler, int group_idx) {
	if (engine->route_count >= HTTP_MAX_ROUTES) {
		qd_string_release(path);
		qd_fatal_raise(ctx, op, "too many routes (at most %d)", HTTP_MAX_ROUTES);
	}
	char* copy = strdup(qd_string_data(path));
	qd_string_release(path);
	if (!copy) {
		qd_fatal_raise(ctx, op, "Out of memory");
	}
	http_route_t* route = &engine->routes[engine->route_count++];
	route->method = method;
	route->path = copy;
	route->handler = handler;
	route->group_idx = group_idx;
}

// Stack signature: ( engine:ptr path:str handler:ptr -- )
static int register_route(qd_context* ctx, const char* op, http_method_t method) {
	void* engine;
	qd_string_t* path;
	void* handler;
	qd_args(ctx, op, "psp", &engine, &path, &handler);
	add_route(ctx, op, (http_engine_t*)engine, method, path, handler, -1);
	return 0;
}

int usr_http_GET(qd_context* ctx) {
	return register_route(ctx, "http::GET", HTTP_METHOD_GET);
}

int usr_http_POST(qd_context* ctx) {
	return register_route(ctx, "http::POST", HTTP_METHOD_POST);
}

int usr_http_PUT(qd_context* ctx) {
	return register_route(ctx, "http::PUT", HTTP_METHOD_PUT);
}

int usr_http_DELETE(qd_context* ctx) {
	return register_route(ctx, "http::DELETE", HTTP_METHOD_DELETE);
}

int usr_http_ANY(qd_context* ctx) {
	return register_route(ctx, "http::ANY", HTTP_METHOD_ANY);
}

// Stack signature: ( engine:ptr middleware:ptr -- )
int usr_http_use(qd_context* ctx) {
	void* handle;
	void* middleware;
	qd_args(ctx, "http::use", "pp", &handle, &middleware);
	http_engine_t* engine = (http_engine_t*)handle;
	if (engine->middleware_count >= HTTP_MAX_MIDDLEWARE) {
		qd_fatal_raise(ctx, "http::use", "too many middlewares (at most %d)", HTTP_MAX_MIDDLEWARE);
	}
	engine->middlewares[engine->middleware_count++] = middleware;
	return 0;
}

/** Thread function for handling a request */
static int request_thread_func(void* arg) {
	http_thread_args_t* args = (http_thread_args_t*)arg;
	qd_context* thread_ctx = qd_create_context(1024);
	handle_request(args->engine, args->client_fd, thread_ctx);
	qd_free_context(thread_ctx);
	free(args);
	return 0;
}

// Read one request into `req`: its head, up to the blank line, then as many body bytes as
// its Content-Length gives. The body may arrive after the head, and a client that sent
// `Expect: 100-continue` sends it only once told to go on. 0 when the request is all
// there, -1 when the client went away, otherwise the status to refuse it with.
static int read_request(int fd, http_buf_t* req, size_t* head_len) {
	char chunk[8192];
	const char* end;
	while (!(end = req->len ? memmem(req->data, req->len, "\r\n\r\n", 4) : NULL)) {
		if (req->len > HTTP_MAX_HEADERS) {
			return 431;
		}
		const ssize_t got = recv(fd, chunk, sizeof(chunk), 0);
		if (got < 0 && errno == EINTR) {
			continue;
		}
		if (got <= 0) {
			return -1;
		}
		if (!http_buf_add(req, chunk, (size_t)got)) {
			return 500;
		}
	}
	*head_len = (size_t)(end - req->data) + 4;
	if (*head_len > HTTP_MAX_HEADERS) {
		return 431;
	}

	size_t body_len = 0;
	size_t value_len;
	const char* cl = http_find_header(req->data, *head_len, "Content-Length", &value_len);
	if (cl) {
		char* digits_end;
		const unsigned long long n = strtoull(cl, &digits_end, 10);
		if (value_len == 0 || digits_end != cl + value_len) {
			return 400;
		}
		if (n > HTTP_MAX_BODY) {
			return 413;
		}
		body_len = (size_t)n;
	}

	const char* expect = http_find_header(req->data, *head_len, "Expect", &value_len);
	if (req->len - *head_len < body_len && expect && value_len == 12 && strncasecmp(expect, "100-continue", 12) == 0) {
		static const char go_on[] = "HTTP/1.1 100 Continue\r\n\r\n";
		if (!send_all(fd, go_on, sizeof(go_on) - 1)) {
			return -1;
		}
	}
	while (req->len - *head_len < body_len) {
		const ssize_t got = recv(fd, chunk, sizeof(chunk), 0);
		if (got < 0 && errno == EINTR) {
			continue;
		}
		if (got <= 0) {
			return -1;
		}
		if (!http_buf_add(req, chunk, (size_t)got)) {
			return 500;
		}
	}
	req->len = *head_len + body_len;
	req->data[req->len] = '\0';
	return 0;
}

// Split the request line "METHOD target HTTP/x" into the context's method, path and query
static bool parse_request_line(const char* head, size_t head_len, http_ctx_t* c) {
	const char* line_end = memmem(head, head_len, "\r\n", 2);
	const char* method_end = memchr(head, ' ', (size_t)(line_end - head));
	if (!method_end || method_end == head) {
		return false;
	}
	const char* target = method_end + 1;
	const char* target_end = memchr(target, ' ', (size_t)(line_end - target));
	if (!target_end || target_end == target) {
		return false;
	}
	const char* query = memchr(target, '?', (size_t)(target_end - target));
	const char* path_end = query ? query : target_end;
	c->method = qd_string_create_with_length(head, (size_t)(method_end - head));
	c->path = qd_string_create_with_length(target, (size_t)(path_end - target));
	c->query = query ? qd_string_create_with_length(query + 1, (size_t)(target_end - query - 1)) : qd_string_create("");
	return c->method && c->path && c->query;
}

static void release_ctx_strings(http_ctx_t* c) {
	qd_string_t* strings[] = {c->method, c->path, c->query, c->headers, c->body, c->params};
	for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); i++) {
		if (strings[i]) {
			qd_string_release(strings[i]);
		}
	}
}

static void run_callback(qd_context* ctx, http_ctx_t* c, void* fn) {
	qd_push_p(ctx, c);
	int (*callback)(qd_context*) = (int (*)(qd_context*))(uintptr_t)fn;
	callback(ctx);
}

/** Internal: Handle a single client request */
static void handle_request(http_engine_t* engine, int client_fd, qd_context* ctx) {
	http_buf_t req = {0};
	size_t head_len = 0;
	const int refused = read_request(client_fd, &req, &head_len);
	http_ctx_t c = {0};
	c.socket = client_fd;
	if (refused > 0) {
		send_text(client_fd, refused, NULL, http_status_text(refused));
	} else if (refused == 0 && !parse_request_line(req.data, head_len, &c)) {
		send_text(client_fd, 400, NULL, "Bad Request");
	} else if (refused == 0) {
		// The header lines, without the request line or the blank line that ends them.
		// A request with no headers ends its request line with that blank line.
		const char* headers = (const char*)memmem(req.data, head_len, "\r\n", 2) + 2;
		const char* headers_end = req.data + head_len - 4;
		c.headers = qd_string_create_with_length(headers, headers < headers_end ? (size_t)(headers_end - headers) : 0);
		c.body = qd_string_create_with_length(req.data + head_len, req.len - head_len);

		// Find matching route
		http_route_t* matched_route = NULL;
		char params[HTTP_MAX_HEADERS * 2] = "";
		const char* path = qd_string_data(c.path);
		const http_method_t req_method = method_from_string(qd_string_data(c.method));
		for (int i = 0; i < engine->route_count && !matched_route; i++) {
			http_route_t* route = &engine->routes[i];
			if (route->method != HTTP_METHOD_ANY && route->method != req_method) {
				continue;
			}
			char full_pattern[HTTP_MAX_URL];
			snprintf(full_pattern, sizeof(full_pattern), "%s%s",
					route->group_idx >= 0 ? engine->groups[route->group_idx].prefix : "", route->path);
			if (match_route(full_pattern, path, params, sizeof(params))) {
				matched_route = route;
			}
		}
		c.params = qd_string_create(params);

		if (!matched_route) {
			send_text(client_fd, 404, NULL, "Not Found");
		} else {
			for (int i = 0; i < engine->middleware_count && !c.responded; i++) {
				run_callback(ctx, &c, engine->middlewares[i]);
			}
			if (matched_route->group_idx >= 0) {
				const http_group_t* group = &engine->groups[matched_route->group_idx];
				for (int i = 0; i < group->middleware_count && !c.responded; i++) {
					run_callback(ctx, &c, group->middlewares[i]);
				}
			}
			if (!c.responded) {
				run_callback(ctx, &c, matched_route->handler);
			}
			if (!c.responded && !c.sse_mode) {
				send_text(client_fd, 500, NULL, "No response sent");
			}
		}
	}

	release_ctx_strings(&c);
	free(c.response_headers.data);
	free(req.data);
	close(client_fd);
}

// Stack signature: ( engine:ptr addr:str -- )!
int usr_http_run(qd_context* ctx) {
	void* handle;
	qd_string_t* addr;
	qd_args(ctx, "http::run", "ps", &handle, &addr);
	http_engine_t* engine = (http_engine_t*)handle;

	char host[256];
	int port;
	const int parsed = parse_addr(qd_string_data(addr), host, sizeof(host), &port);
	qd_string_release(addr);
	if (parsed < 0) {
		return qd_fail(ctx, HTTP_ERR_BIND, "http::run: could not parse the listen address");
	}

	struct sockaddr_in server_addr = {0};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons((uint16_t)port);
	if (strcmp(host, "0.0.0.0") == 0) {
		server_addr.sin_addr.s_addr = INADDR_ANY;
	} else if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
		return qd_fail(ctx, HTTP_ERR_BIND, "http::run: %s is not a valid IPv4 address", host);
	}

	const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		return qd_fail(ctx, HTTP_ERR_BIND, "http::run: could not create the server socket: %s", strerror(errno));
	}
	int opt = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 || listen(server_fd, 128) < 0) {
		const int err = errno;
		close(server_fd);
		return qd_fail(ctx, HTTP_ERR_BIND, "http::run: could not listen on %s:%d: %s", host, port, strerror(err));
	}

	engine->server_fd = server_fd;
	atomic_store(&engine->running, true);

	while (atomic_load(&engine->running)) {
		const int client_fd = accept(server_fd, NULL, NULL);
		if (client_fd < 0) {
			continue;
		}

		// Each request runs in its own thread, with its own context
		http_thread_args_t* args = malloc(sizeof(http_thread_args_t));
		thrd_t thread;
		if (args) {
			args->engine = engine;
			args->client_fd = client_fd;
		}
		if (args && thrd_create(&thread, request_thread_func, args) == thrd_success) {
			thrd_detach(thread);
		} else {
			free(args);
			handle_request(engine, client_fd, ctx);
		}
	}

	close(server_fd);
	engine->server_fd = -1;
	return qd_push_i(ctx, HTTP_OK);
}

// Stack signature: ( engine:ptr -- handled:i64 )
int usr_http_handle_one(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "http::handle_one", "p", &handle);
	http_engine_t* engine = (http_engine_t*)handle;
	if (engine->server_fd < 0) {
		return qd_push_i(ctx, 0);
	}

	// Poll for incoming connection (non-blocking)
	struct pollfd pfd = {.fd = engine->server_fd, .events = POLLIN};
	if (poll(&pfd, 1, 0) <= 0) {
		return qd_push_i(ctx, 0);
	}
	const int client_fd = accept(engine->server_fd, NULL, NULL);
	if (client_fd < 0) {
		return qd_push_i(ctx, 0);
	}
	handle_request(engine, client_fd, ctx);
	return qd_push_i(ctx, 1);
}

// Stack signature: ( engine:ptr -- )
int usr_http_stop(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "http::stop", "p", &handle);
	http_engine_t* engine = (http_engine_t*)handle;
	atomic_store(&engine->running, false);
	// Unblocks the accept in http::run
	if (engine->server_fd >= 0) {
		shutdown(engine->server_fd, SHUT_RDWR);
	}
	return 0;
}

// Stack signature: ( engine:ptr -- )
int usr_http_free_engine(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "http::free_engine", "p", &handle);
	http_engine_t* engine = (http_engine_t*)handle;
	if (engine->server_fd >= 0) {
		close(engine->server_fd);
	}
	for (int i = 0; i < engine->route_count; i++) {
		free(engine->routes[i].path);
	}
	for (int i = 0; i < engine->group_count; i++) {
		free(engine->groups[i].prefix);
	}
	for (int i = 0; i < engine->group_handle_count; i++) {
		free(engine->group_handles[i]);
	}
	free(engine);
	return 0;
}

// ============================================================
// Context Methods
// ============================================================

static void push_found(qd_context* ctx, const char* value, size_t len) {
	qd_string_t* s = qd_string_create_with_length(value ? value : "", value ? len : 0);
	qd_push_s_ref(ctx, s);
	qd_string_release(s);
}

// Stack signature: ( c:Ctx name:str -- value:str )
int usr_http_param(qd_context* ctx) {
	void* handle;
	qd_string_t* name;
	qd_args(ctx, "http::param", "ps", &handle, &name);
	char value[HTTP_MAX_HEADERS];
	find_param(qd_string_data(((http_ctx_t*)handle)->params), qd_string_data(name), value, sizeof(value));
	qd_string_release(name);
	return qd_push_s(ctx, value);
}

// Stack signature: ( c:Ctx name:str -- value:str )
int usr_http_query_param(qd_context* ctx) {
	void* handle;
	qd_string_t* name;
	qd_args(ctx, "http::query_param", "ps", &handle, &name);
	char value[HTTP_MAX_HEADERS];
	find_query_param(qd_string_data(((http_ctx_t*)handle)->query), qd_string_data(name), value, sizeof(value));
	qd_string_release(name);
	return qd_push_s(ctx, value);
}

// Stack signature: ( c:Ctx -- body:str )
int usr_http_request_body(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "http::request_body", "p", &handle);
	return qd_push_s_ref(ctx, ((http_ctx_t*)handle)->body);
}

// Stack signature: ( c:Ctx name:str -- value:str )
int usr_http_get_header(qd_context* ctx) {
	void* handle;
	qd_string_t* name;
	qd_args(ctx, "http::get_header", "ps", &handle, &name);
	const qd_string_t* headers = ((http_ctx_t*)handle)->headers;
	size_t len = 0;
	const char* value =
			http_find_header(qd_string_data(headers), qd_string_length(headers), qd_string_data(name), &len);
	qd_string_release(name);
	push_found(ctx, value, len);
	return 0;
}

// Stack signature: ( c:Ctx status:i64 body:str -- )
static int respond(qd_context* ctx, const char* op, const char* content_type) {
	void* handle;
	int64_t status;
	qd_string_t* body;
	qd_args(ctx, op, "pis", &handle, &status, &body);
	http_ctx_t* c = (http_ctx_t*)handle;
	if (!c->responded) {
		send_response((int)c->socket, (int)status, &c->response_headers, content_type, qd_string_data(body),
				qd_string_length(body));
		c->responded = 1;
	}
	qd_string_release(body);
	return 0;
}

int usr_http_string(qd_context* ctx) {
	return respond(ctx, "http::string", "text/plain; charset=utf-8");
}

int usr_http_json(qd_context* ctx) {
	return respond(ctx, "http::json", "application/json; charset=utf-8");
}

int usr_http_html(qd_context* ctx) {
	return respond(ctx, "http::html", "text/html; charset=utf-8");
}

int usr_http_abort(qd_context* ctx) {
	return respond(ctx, "http::abort", "text/plain; charset=utf-8");
}

// Stack signature: ( c:Ctx name:str value:str -- )
int usr_http_set_header(qd_context* ctx) {
	void* handle;
	qd_string_t* name;
	qd_string_t* value;
	qd_args(ctx, "http::set_header", "pss", &handle, &name, &value);
	reject_line_break(ctx, "http::set_header", name, value);
	http_ctx_t* c = (http_ctx_t*)handle;
	const bool ok = http_buf_printf(&c->response_headers, "%s: %s\r\n", qd_string_data(name), qd_string_data(value));
	qd_string_release(name);
	qd_string_release(value);
	if (!ok) {
		qd_fatal_raise(ctx, "http::set_header", "Out of memory");
	}
	return 0;
}

// Stack signature: ( c:Ctx -- sent:i64 )
int usr_http_is_responded(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "http::is_responded", "p", &handle);
	return qd_push_i(ctx, ((http_ctx_t*)handle)->responded);
}

// ============================================================
// Route Groups
// ============================================================

// A group is handed out as an (engine, group index) pair
typedef struct {
	http_engine_t* engine;
	int index;
} http_group_handle_t;

// Stack signature: ( engine:ptr prefix:str -- group:ptr )
int usr_http_group(qd_context* ctx) {
	void* handle;
	qd_string_t* prefix;
	qd_args(ctx, "http::group", "ps", &handle, &prefix);
	http_engine_t* engine = (http_engine_t*)handle;
	if (engine->group_count >= HTTP_MAX_GROUPS) {
		qd_string_release(prefix);
		qd_fatal_raise(ctx, "http::group", "too many route groups (at most %d)", HTTP_MAX_GROUPS);
	}
	char* copy = strdup(qd_string_data(prefix));
	qd_string_release(prefix);
	http_group_handle_t* group_handle = malloc(sizeof(http_group_handle_t));
	if (!copy || !group_handle) {
		free(copy);
		free(group_handle);
		qd_fatal_raise(ctx, "http::group", "Out of memory");
	}
	http_group_t* group = &engine->groups[engine->group_count];
	group->prefix = copy;
	group->middleware_count = 0;
	group_handle->engine = engine;
	group_handle->index = engine->group_count++;
	engine->group_handles[engine->group_handle_count++] = group_handle;
	return qd_push_p(ctx, group_handle);
}

// Stack signature: ( group:ptr middleware:ptr -- )
int usr_http_group_use(qd_context* ctx) {
	void* handle;
	void* middleware;
	qd_args(ctx, "http::group_use", "pp", &handle, &middleware);
	const http_group_handle_t* g = (const http_group_handle_t*)handle;
	http_group_t* group = &g->engine->groups[g->index];
	if (group->middleware_count >= HTTP_MAX_MIDDLEWARE) {
		qd_fatal_raise(ctx, "http::group_use", "too many middlewares in group (at most %d)", HTTP_MAX_MIDDLEWARE);
	}
	group->middlewares[group->middleware_count++] = middleware;
	return 0;
}

// Stack signature: ( group:ptr path:str handler:ptr -- )
static int register_group_route(qd_context* ctx, const char* op, http_method_t method) {
	void* handle;
	qd_string_t* path;
	void* handler;
	qd_args(ctx, op, "psp", &handle, &path, &handler);
	const http_group_handle_t* g = (const http_group_handle_t*)handle;
	add_route(ctx, op, g->engine, method, path, handler, g->index);
	return 0;
}

int usr_http_group_GET(qd_context* ctx) {
	return register_group_route(ctx, "http::group_GET", HTTP_METHOD_GET);
}

int usr_http_group_POST(qd_context* ctx) {
	return register_group_route(ctx, "http::group_POST", HTTP_METHOD_POST);
}

int usr_http_group_PUT(qd_context* ctx) {
	return register_group_route(ctx, "http::group_PUT", HTTP_METHOD_PUT);
}

int usr_http_group_DELETE(qd_context* ctx) {
	return register_group_route(ctx, "http::group_DELETE", HTTP_METHOD_DELETE);
}

// ============================================================
// Static File Serving
// ============================================================

/** Get MIME type from file extension */
static const char* get_mime_type(const char* path) {
	const char* ext = strrchr(path, '.');
	if (!ext) {
		return "application/octet-stream";
	}
	ext++; // Skip the dot

	// Common web types
	if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
		return "text/html; charset=utf-8";
	}
	if (strcasecmp(ext, "css") == 0) {
		return "text/css; charset=utf-8";
	}
	if (strcasecmp(ext, "js") == 0) {
		return "application/javascript; charset=utf-8";
	}
	if (strcasecmp(ext, "json") == 0) {
		return "application/json; charset=utf-8";
	}
	if (strcasecmp(ext, "xml") == 0) {
		return "application/xml; charset=utf-8";
	}
	if (strcasecmp(ext, "txt") == 0) {
		return "text/plain; charset=utf-8";
	}
	if (strcasecmp(ext, "csv") == 0) {
		return "text/csv; charset=utf-8";
	}

	// Images
	if (strcasecmp(ext, "png") == 0) {
		return "image/png";
	}
	if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
		return "image/jpeg";
	}
	if (strcasecmp(ext, "gif") == 0) {
		return "image/gif";
	}
	if (strcasecmp(ext, "svg") == 0) {
		return "image/svg+xml";
	}
	if (strcasecmp(ext, "ico") == 0) {
		return "image/x-icon";
	}
	if (strcasecmp(ext, "webp") == 0) {
		return "image/webp";
	}

	// Fonts
	if (strcasecmp(ext, "woff") == 0) {
		return "font/woff";
	}
	if (strcasecmp(ext, "woff2") == 0) {
		return "font/woff2";
	}
	if (strcasecmp(ext, "ttf") == 0) {
		return "font/ttf";
	}
	if (strcasecmp(ext, "otf") == 0) {
		return "font/otf";
	}
	if (strcasecmp(ext, "eot") == 0) {
		return "application/vnd.ms-fontobject";
	}

	// Audio/Video
	if (strcasecmp(ext, "mp3") == 0) {
		return "audio/mpeg";
	}
	if (strcasecmp(ext, "wav") == 0) {
		return "audio/wav";
	}
	if (strcasecmp(ext, "mp4") == 0) {
		return "video/mp4";
	}
	if (strcasecmp(ext, "webm") == 0) {
		return "video/webm";
	}
	if (strcasecmp(ext, "ogg") == 0) {
		return "audio/ogg";
	}

	// Documents
	if (strcasecmp(ext, "pdf") == 0) {
		return "application/pdf";
	}
	if (strcasecmp(ext, "zip") == 0) {
		return "application/zip";
	}
	if (strcasecmp(ext, "gz") == 0) {
		return "application/gzip";
	}
	if (strcasecmp(ext, "tar") == 0) {
		return "application/x-tar";
	}

	// WebAssembly
	if (strcasecmp(ext, "wasm") == 0) {
		return "application/wasm";
	}

	return "application/octet-stream";
}

/** Send a file as the response; false when it cannot be read */
static bool send_file_response(int socket, const char* filepath, const http_buf_t* headers) {
	FILE* f = fopen(filepath, "rb");
	if (!f) {
		return false;
	}
	http_buf_t data = {0};
	char chunk[8192];
	size_t got;
	bool ok = true;
	while (ok && (got = fread(chunk, 1, sizeof(chunk), f)) > 0) {
		ok = http_buf_add(&data, chunk, got);
	}
	ok = ok && !ferror(f);
	fclose(f);
	if (ok) {
		// The file was read; a failure from here on is the client's connection
		send_response(socket, 200, headers, get_mime_type(filepath), data.data ? data.data : "", data.len);
	}
	free(data.data);
	return ok;
}

// Stack signature: ( c:Ctx filepath:str -- )
int usr_http_static_file(qd_context* ctx) {
	void* handle;
	qd_string_t* path;
	qd_args(ctx, "http::static_file", "ps", &handle, &path);
	http_ctx_t* c = (http_ctx_t*)handle;
	const char* filepath = qd_string_data(path);
	if (!c->responded) {
		if (strstr(filepath, "..") != NULL) {
			send_text((int)c->socket, 403, &c->response_headers, "Forbidden");
		} else if (!send_file_response((int)c->socket, filepath, &c->response_headers)) {
			send_text((int)c->socket, 404, &c->response_headers, "Not Found");
		}
		c->responded = 1;
	}
	qd_string_release(path);
	return 0;
}

// Confine a candidate filesystem path to a root directory. Both are
// canonicalized with realpath (which resolves "..", ".", and symlinks), then
// the canonical candidate must lie within the canonical root. Returns 1 if the
// path is safe to serve, 0 if it does not resolve or escapes the root. On
// success the canonical path is written to resolved_out (size PATH_MAX).
// Percent-encoded traversal (e.g. %2e%2e) is handled implicitly: it resolves to
// a literal, non-existent filename and realpath fails.
static int resolve_within_root(const char* root, const char* candidate, char* resolved_out) {
	char root_real[PATH_MAX];
	if (realpath(root, root_real) == NULL) {
		return 0;
	}
	if (realpath(candidate, resolved_out) == NULL) {
		return 0; // missing file or unresolved traversal
	}
	size_t rlen = strlen(root_real);
	if (strncmp(resolved_out, root_real, rlen) != 0) {
		return 0;
	}
	// Must be the root itself or a path strictly under it.
	return resolved_out[rlen] == '\0' || resolved_out[rlen] == '/';
}

// The file under `fs_path` that the request path names past `prefix`, or the status to
// refuse it with
static int static_target(const char* fs_path, const char* prefix, const char* request_path, char* resolved) {
	const size_t prefix_len = strlen(prefix);
	if (strncmp(request_path, prefix, prefix_len) != 0) {
		return 404;
	}
	const char* rel_path = request_path + prefix_len;
	if (*rel_path == '/') {
		rel_path++;
	}
	// A second slash would make it absolute, ignoring fs_path
	if (*rel_path == '/') {
		return 403;
	}

	char filepath[4096];
	const size_t fs_path_len = strlen(fs_path);
	snprintf(filepath, sizeof(filepath), "%s%s%s", fs_path,
			(fs_path_len > 0 && fs_path[fs_path_len - 1] == '/') ? "" : "/", rel_path);
	const size_t filepath_len = strlen(filepath);
	if (filepath_len == 0 || filepath[filepath_len - 1] == '/') {
		strncat(filepath, "index.html", sizeof(filepath) - filepath_len - 1);
	}

	// Confine the resolved path to fs_path. This canonicalizes away "..", ".", and
	// symlinks and rejects anything that escapes the configured root. A file that
	// does not exist fails realpath with ENOENT: 404 rather than 403.
	if (!resolve_within_root(fs_path, filepath, resolved)) {
		return (errno == ENOENT || errno == ENOTDIR) ? 404 : 403;
	}
	return 0;
}

// Stack signature: ( c:Ctx prefix:str fs_path:str -- )
int usr_http_static(qd_context* ctx) {
	void* handle;
	qd_string_t* prefix;
	qd_string_t* fs_path;
	qd_args(ctx, "http::static", "pss", &handle, &prefix, &fs_path);
	http_ctx_t* c = (http_ctx_t*)handle;
	if (!c->responded) {
		char resolved[PATH_MAX];
		int status = static_target(qd_string_data(fs_path), qd_string_data(prefix), qd_string_data(c->path), resolved);
		if (status == 0 && !send_file_response((int)c->socket, resolved, &c->response_headers)) {
			status = 404;
		}
		if (status != 0) {
			send_text((int)c->socket, status, &c->response_headers, status == 404 ? "Not Found" : "Forbidden");
		}
		c->responded = 1;
	}
	qd_string_release(prefix);
	qd_string_release(fs_path);
	return 0;
}

// ============================================================
// Server-Sent Events (SSE)
// ============================================================

// Stack signature: ( c:Ctx -- )
int usr_http_sse_start(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "http::sse_start", "p", &handle);
	http_ctx_t* c = (http_ctx_t*)handle;
	if (c->responded || c->sse_mode) {
		return 0;
	}
	if (send_head((int)c->socket, 200, "text/event-stream", -1, "Cache-Control: no-cache\r\nConnection: keep-alive\r\n",
				&c->response_headers)) {
		c->sse_mode = 1;
	} else {
		c->responded = 1;
	}
	return 0;
}

// Send one event, or end the stream when the client has gone
static void send_event(qd_context* ctx, const char* op, http_ctx_t* c, const char* name, const char* data) {
	http_buf_t event = {0};
	const bool built =
			(!name || http_buf_printf(&event, "event: %s\n", name)) && http_buf_printf(&event, "data: %s\n\n", data);
	if (!built) {
		free(event.data);
		qd_fatal_raise(ctx, op, "Out of memory");
	}
	if (!send_all((int)c->socket, event.data, event.len)) {
		c->sse_mode = 0;
		c->responded = 1;
	}
	free(event.data);
}

// Stack signature: ( c:Ctx data:str -- )
int usr_http_sse_send(qd_context* ctx) {
	void* handle;
	qd_string_t* data;
	qd_args(ctx, "http::sse_send", "ps", &handle, &data);
	http_ctx_t* c = (http_ctx_t*)handle;
	if (c->sse_mode) {
		send_event(ctx, "http::sse_send", c, NULL, qd_string_data(data));
	}
	qd_string_release(data);
	return 0;
}

// Stack signature: ( c:Ctx name:str data:str -- )
int usr_http_sse_event(qd_context* ctx) {
	void* handle;
	qd_string_t* name;
	qd_string_t* data;
	qd_args(ctx, "http::sse_event", "pss", &handle, &name, &data);
	http_ctx_t* c = (http_ctx_t*)handle;
	if (c->sse_mode) {
		send_event(ctx, "http::sse_event", c, qd_string_data(name), qd_string_data(data));
	}
	qd_string_release(name);
	qd_string_release(data);
	return 0;
}

// Stack signature: ( c:Ctx -- )
int usr_http_sse_end(qd_context* ctx) {
	void* handle;
	qd_args(ctx, "http::sse_end", "p", &handle);
	http_ctx_t* c = (http_ctx_t*)handle;
	if (c->sse_mode) {
		c->sse_mode = 0;
		c->responded = 1;
	}
	return 0;
}
