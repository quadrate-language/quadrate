#define _DEFAULT_SOURCE
/**
 * @file http_server.c
 * @brief HTTP server implementation for Quadrate (Gin-inspired API)
 */

#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <quadrate/http/http.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
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
	bool running;

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

	// Response headers buffer (per-request)
	char* response_headers;
	size_t response_headers_len;
	size_t response_headers_cap;
} http_engine_t;

/** HTTP Context (matches Quadrate Ctx struct layout) */
typedef struct {
	struct qd_string* method;
	struct qd_string* path;
	struct qd_string* query;
	struct qd_string* headers;
	struct qd_string* body;
	struct qd_string* params;
	int64_t socket;
	int64_t responded;
	// Internal: response headers buffer (not exposed to Quadrate)
	char* response_headers;
	size_t response_headers_len;
	size_t response_headers_cap;
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
			size_t needed = (params_len > 0 ? 1 : 0) + 5 + 1 + rest_len + 1; // "_path=..."
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

/** Parse HTTP request */
static int parse_request(const char* data, size_t len, char* method_out, char* path_out, char* query_out,
		char* headers_out, char* body_out) {
	if (len < 14) {
		return -1; // Minimum: "GET / HTTP/1.0"
	}

	// Parse request line
	const char* p = data;
	const char* line_end = strstr(p, "\r\n");
	if (!line_end) {
		return -1;
	}

	// Method
	const char* method_end = strchr(p, ' ');
	if (!method_end || method_end > line_end) {
		return -1;
	}
	size_t method_len = (size_t)(method_end - p);
	if (method_len > 15) {
		return -1;
	}
	memcpy(method_out, p, method_len);
	method_out[method_len] = '\0';

	// Path (and query)
	p = method_end + 1;
	const char* path_end = strchr(p, ' ');
	if (!path_end || path_end > line_end) {
		return -1;
	}

	const char* query_start = strchr(p, '?');
	if (query_start && query_start < path_end) {
		size_t path_len = (size_t)(query_start - p);
		memcpy(path_out, p, path_len);
		path_out[path_len] = '\0';

		size_t query_len = (size_t)(path_end - query_start - 1);
		memcpy(query_out, query_start + 1, query_len);
		query_out[query_len] = '\0';
	} else {
		size_t path_len = (size_t)(path_end - p);
		memcpy(path_out, p, path_len);
		path_out[path_len] = '\0';
		query_out[0] = '\0';
	}

	// Headers
	p = line_end + 2;
	const char* headers_end = strstr(p, "\r\n\r\n");
	if (!headers_end) {
		// No body
		size_t headers_len = len - (size_t)(p - data);
		memcpy(headers_out, p, headers_len);
		headers_out[headers_len] = '\0';
		body_out[0] = '\0';
	} else {
		size_t headers_len = (size_t)(headers_end - p);
		memcpy(headers_out, p, headers_len);
		headers_out[headers_len] = '\0';

		// Body
		const char* body_start = headers_end + 4;
		size_t body_len = len - (size_t)(body_start - data);
		memcpy(body_out, body_start, body_len);
		body_out[body_len] = '\0';
	}

	return 0;
}

/** Find header value (case-insensitive) */
static const char* find_header(const char* headers, const char* name, char* value_out, size_t max_len) {
	size_t name_len = strlen(name);
	const char* p = headers;

	while (*p) {
		// Find line end
		const char* line_end = strstr(p, "\r\n");
		if (!line_end) {
			line_end = p + strlen(p);
		}

		// Check if this line matches
		if (strncasecmp(p, name, name_len) == 0 && p[name_len] == ':') {
			const char* val = p + name_len + 1;
			while (*val == ' ') {
				val++;
			}
			size_t val_len = (size_t)(line_end - val);
			if (val_len >= max_len) {
				val_len = max_len - 1;
			}
			memcpy(value_out, val, val_len);
			value_out[val_len] = '\0';
			return value_out;
		}

		// Next line
		if (*line_end == '\0') {
			break;
		}
		p = line_end + 2;
	}

	value_out[0] = '\0';
	return NULL;
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

/** Send HTTP response */
static int send_response(
		int socket, int status, const char* extra_headers, const char* content_type, const char* body) {
	char header[4096];
	size_t body_len = body ? strlen(body) : 0;

	int header_len = snprintf(header, sizeof(header),
			"HTTP/1.1 %d %s\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %zu\r\n"
			"Connection: close\r\n"
			"%s"
			"\r\n",
			status, http_status_text(status), content_type, body_len, extra_headers ? extra_headers : "");

	if (send(socket, header, (size_t)header_len, 0) < 0) {
		return -1;
	}
	if (body_len > 0 && send(socket, body, body_len, 0) < 0) {
		return -1;
	}

	return 0;
}

// ============================================================
// Engine API
// ============================================================

int usr_http_engine(qd_context* ctx) {
	http_engine_t* engine = calloc(1, sizeof(http_engine_t));
	if (!engine) {
		fprintf(stderr, "Fatal error in http::engine: memory allocation failed\n");
		abort();
	}

	engine->server_fd = -1;
	engine->running = false;
	engine->route_count = 0;
	engine->middleware_count = 0;
	engine->group_count = 0;
	engine->group_handle_count = 0;
	engine->response_headers = NULL;
	engine->response_headers_len = 0;
	engine->response_headers_cap = 0;

	qd_push_p(ctx, engine);
	return 0;
}

/** Internal: Register route with method */
static int register_route(qd_context* ctx, http_method_t method) {
	// Pop handler
	qd_stack_element_t handler_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &handler_elem);
	if (err != QD_STACK_OK || handler_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in http route registration: expected handler function\n");
		abort();
	}

	// Pop path
	qd_stack_element_t path_elem;
	err = qd_stack_pop(ctx->st, &path_elem);
	if (err != QD_STACK_OK || path_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http route registration: expected path string\n");
		abort();
	}

	// Pop engine
	qd_stack_element_t engine_elem;
	err = qd_stack_pop(ctx->st, &engine_elem);
	if (err != QD_STACK_OK || engine_elem.type != QD_STACK_TYPE_PTR) {
		qd_string_release(path_elem.value.s);
		fprintf(stderr, "Fatal error in http route registration: expected engine pointer\n");
		abort();
	}

	http_engine_t* engine = (http_engine_t*)engine_elem.value.p;

	if (engine->route_count >= HTTP_MAX_ROUTES) {
		qd_string_release(path_elem.value.s);
		fprintf(stderr, "Fatal error: too many routes\n");
		abort();
	}

	http_route_t* route = &engine->routes[engine->route_count++];
	route->method = method;
	route->path = strdup(qd_string_data(path_elem.value.s));
	if (!route->path) {
		engine->route_count--; // Rollback
		qd_string_release(path_elem.value.s);
		fprintf(stderr, "Fatal error in http route registration: memory allocation failed\n");
		abort();
	}
	route->handler = handler_elem.value.p;
	route->group_idx = -1;

	qd_string_release(path_elem.value.s);
	return 0;
}

int usr_http_GET(qd_context* ctx) {
	return register_route(ctx, HTTP_METHOD_GET);
}

int usr_http_POST(qd_context* ctx) {
	return register_route(ctx, HTTP_METHOD_POST);
}

int usr_http_PUT(qd_context* ctx) {
	return register_route(ctx, HTTP_METHOD_PUT);
}

int usr_http_DELETE(qd_context* ctx) {
	return register_route(ctx, HTTP_METHOD_DELETE);
}

int usr_http_ANY(qd_context* ctx) {
	return register_route(ctx, HTTP_METHOD_ANY);
}

int usr_http_use(qd_context* ctx) {
	// Pop middleware
	qd_stack_element_t mw_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &mw_elem);
	if (err != QD_STACK_OK || mw_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in http::use: expected middleware function\n");
		abort();
	}

	// Pop engine
	qd_stack_element_t engine_elem;
	err = qd_stack_pop(ctx->st, &engine_elem);
	if (err != QD_STACK_OK || engine_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in http::use: expected engine pointer\n");
		abort();
	}

	http_engine_t* engine = (http_engine_t*)engine_elem.value.p;

	if (engine->middleware_count >= HTTP_MAX_MIDDLEWARE) {
		fprintf(stderr, "Fatal error: too many middlewares\n");
		abort();
	}

	engine->middlewares[engine->middleware_count++] = mw_elem.value.p;
	return 0;
}

/** Thread function for handling a request */
static int request_thread_func(void* arg) {
	http_thread_args_t* args = (http_thread_args_t*)arg;

	// Create a new context for this thread
	qd_context* thread_ctx = qd_create_context(1024);

	// Handle the request
	handle_request(args->engine, args->client_fd, thread_ctx);

	// Clean up
	qd_free_context(thread_ctx);
	free(args);

	return 0;
}

/** Internal: Handle a single client request */
static void handle_request(http_engine_t* engine, int client_fd, qd_context* ctx) {
	// Receive request
	char buffer[65536];
	ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (received <= 0) {
		close(client_fd);
		return;
	}
	buffer[received] = '\0';

	// Parse request
	char method[16], path[4096], query[4096], headers[8192], body[32768];
	if (parse_request(buffer, (size_t)received, method, path, query, headers, body) < 0) {
		send_response(client_fd, 400, NULL, "text/plain", "Bad Request");
		close(client_fd);
		return;
	}

	// Find matching route
	http_route_t* matched_route = NULL;
	char params[4096] = "";
	http_method_t req_method = method_from_string(method);

	for (int i = 0; i < engine->route_count; i++) {
		http_route_t* route = &engine->routes[i];

		// Check method
		if (route->method != HTTP_METHOD_ANY && route->method != req_method) {
			continue;
		}

		// Build full path for grouped routes
		char full_pattern[4096];
		if (route->group_idx >= 0) {
			snprintf(full_pattern, sizeof(full_pattern), "%s%s", engine->groups[route->group_idx].prefix, route->path);
		} else {
			strncpy(full_pattern, route->path, sizeof(full_pattern) - 1);
			full_pattern[sizeof(full_pattern) - 1] = '\0';
		}

		// Match
		if (match_route(full_pattern, path, params, sizeof(params))) {
			matched_route = route;
			break;
		}
	}

	// Create context struct
	http_ctx_t* http_ctx = malloc(sizeof(http_ctx_t));
	if (!http_ctx) {
		send_response(client_fd, 500, NULL, "text/plain", "Internal Server Error");
		close(client_fd);
		return;
	}
	http_ctx->method = qd_string_create(method);
	http_ctx->path = qd_string_create(path);
	http_ctx->query = qd_string_create(query);
	http_ctx->headers = qd_string_create(headers);
	http_ctx->body = qd_string_create(body);
	http_ctx->params = qd_string_create(params);
	http_ctx->socket = client_fd;
	http_ctx->responded = 0;
	http_ctx->response_headers = NULL;
	http_ctx->response_headers_len = 0;
	http_ctx->response_headers_cap = 0;
	http_ctx->sse_mode = 0;

	// Note: response_headers in engine is NOT thread-safe.
	// Each request runs in its own thread, so we don't use shared state.
	(void)engine->response_headers; // Intentionally unused in threaded mode

	if (!matched_route) {
		// 404
		send_response(client_fd, 404, NULL, "text/plain", "Not Found");
		http_ctx->responded = 1;
	} else {
		// Run global middleware
		for (int i = 0; i < engine->middleware_count && !http_ctx->responded; i++) {
			// Push context as struct fields
			qd_push_p(ctx, http_ctx);
			// Call middleware
			void (*mw)(qd_context*) = (void (*)(qd_context*))engine->middlewares[i];
			mw(ctx);
		}

		// Run group middleware
		if (matched_route->group_idx >= 0 && !http_ctx->responded) {
			http_group_t* group = &engine->groups[matched_route->group_idx];
			for (int i = 0; i < group->middleware_count && !http_ctx->responded; i++) {
				qd_push_p(ctx, http_ctx);
				void (*mw)(qd_context*) = (void (*)(qd_context*))group->middlewares[i];
				mw(ctx);
			}
		}

		// Run handler
		if (!http_ctx->responded) {
			qd_push_p(ctx, http_ctx);
			void (*handler)(qd_context*) = (void (*)(qd_context*))matched_route->handler;
			handler(ctx);
		}

		// If still not responded (and not in SSE mode), send 500
		if (!http_ctx->responded && !http_ctx->sse_mode) {
			send_response(client_fd, 500, NULL, "text/plain", "No response sent");
		}
	}

	// Clean up
	qd_string_release(http_ctx->method);
	qd_string_release(http_ctx->path);
	qd_string_release(http_ctx->query);
	qd_string_release(http_ctx->headers);
	qd_string_release(http_ctx->body);
	qd_string_release(http_ctx->params);
	free(http_ctx->response_headers);
	free(http_ctx);

	close(client_fd);
}

int usr_http_run(qd_context* ctx) {
	// Pop addr
	qd_stack_element_t addr_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &addr_elem);
	if (err != QD_STACK_OK || addr_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::run: expected address string\n");
		abort();
	}

	// Pop engine
	qd_stack_element_t engine_elem;
	err = qd_stack_pop(ctx->st, &engine_elem);
	if (err != QD_STACK_OK || engine_elem.type != QD_STACK_TYPE_PTR) {
		qd_string_release(addr_elem.value.s);
		fprintf(stderr, "Fatal error in http::run: expected engine pointer\n");
		abort();
	}

	http_engine_t* engine = (http_engine_t*)engine_elem.value.p;
	const char* addr = qd_string_data(addr_elem.value.s);

	// Parse address
	char host[256];
	int port;
	if (parse_addr(addr, host, sizeof(host), &port) < 0) {
		qd_string_release(addr_elem.value.s);
		qd_push_i(ctx, HTTP_ERR_BIND);
		return (int){HTTP_ERR_BIND};
	}

	// Create server socket
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		qd_string_release(addr_elem.value.s);
		qd_push_i(ctx, HTTP_ERR_BIND);
		return (int){HTTP_ERR_BIND};
	}

	// Set SO_REUSEADDR
	int opt = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// Bind
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons((uint16_t)port);

	if (strcmp(host, "0.0.0.0") == 0) {
		server_addr.sin_addr.s_addr = INADDR_ANY;
	} else {
		if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
			close(server_fd);
			qd_string_release(addr_elem.value.s);
			qd_push_i(ctx, HTTP_ERR_BIND);
			return (int){HTTP_ERR_BIND};
		}
	}

	if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		close(server_fd);
		qd_string_release(addr_elem.value.s);
		qd_push_i(ctx, HTTP_ERR_BIND);
		return (int){HTTP_ERR_BIND};
	}

	// Listen
	if (listen(server_fd, 128) < 0) {
		close(server_fd);
		qd_string_release(addr_elem.value.s);
		qd_push_i(ctx, HTTP_ERR_BIND);
		return (int){HTTP_ERR_BIND};
	}

	engine->server_fd = server_fd;
	engine->running = true;

	qd_string_release(addr_elem.value.s);

	// Accept loop
	while (engine->running) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);

		int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
		if (client_fd < 0) {
			if (!engine->running) {
				break;
			}
			continue;
		}

		// Spawn a new thread for each request
		http_thread_args_t* args = malloc(sizeof(http_thread_args_t));
		if (args == NULL) {
			close(client_fd);
			continue;
		}
		args->engine = engine;
		args->client_fd = client_fd;

		thrd_t thread;
		if (thrd_create(&thread, request_thread_func, args) != thrd_success) {
			// If thread creation fails, fall back to synchronous handling
			free(args);
			handle_request(engine, client_fd, ctx);
		} else {
			// Detach thread so it cleans up automatically
			thrd_detach(thread);
		}
	}

	close(server_fd);
	engine->server_fd = -1;

	qd_push_i(ctx, HTTP_OK);
	return 0;
}

int usr_http_handle_one(qd_context* ctx) {
	// Pop engine
	qd_stack_element_t engine_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &engine_elem);
	if (err != QD_STACK_OK || engine_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in http::handle_one: expected engine pointer\n");
		abort();
	}

	http_engine_t* engine = (http_engine_t*)engine_elem.value.p;

	if (engine->server_fd < 0) {
		qd_push_i(ctx, 0);
		return 0;
	}

	// Poll for incoming connection (non-blocking)
	struct pollfd pfd;
	pfd.fd = engine->server_fd;
	pfd.events = POLLIN;

	int ret = poll(&pfd, 1, 0); // 0 timeout = non-blocking
	if (ret <= 0) {
		qd_push_i(ctx, 0);
		return 0;
	}

	// Accept
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(engine->server_fd, (struct sockaddr*)&client_addr, &client_len);
	if (client_fd < 0) {
		qd_push_i(ctx, 0);
		return 0;
	}

	handle_request(engine, client_fd, ctx);

	qd_push_i(ctx, 1);
	return 0;
}

int usr_http_stop(qd_context* ctx) {
	// Pop engine
	qd_stack_element_t engine_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &engine_elem);
	if (err != QD_STACK_OK || engine_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in http::stop: expected engine pointer\n");
		abort();
	}

	http_engine_t* engine = (http_engine_t*)engine_elem.value.p;
	engine->running = false;

	// Close server socket to unblock accept()
	if (engine->server_fd >= 0) {
		shutdown(engine->server_fd, SHUT_RDWR);
	}

	return 0;
}

int usr_http_free_engine(qd_context* ctx) {
	// Pop engine
	qd_stack_element_t engine_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &engine_elem);
	if (err != QD_STACK_OK || engine_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in http::free_engine: expected engine pointer\n");
		abort();
	}

	http_engine_t* engine = (http_engine_t*)engine_elem.value.p;

	// Close socket if still open
	if (engine->server_fd >= 0) {
		close(engine->server_fd);
	}

	// Free routes
	for (int i = 0; i < engine->route_count; i++) {
		free(engine->routes[i].path);
	}

	// Free groups
	for (int i = 0; i < engine->group_count; i++) {
		free(engine->groups[i].prefix);
	}

	// Free group handles
	for (int i = 0; i < engine->group_handle_count; i++) {
		free(engine->group_handles[i]);
	}

	// Free response headers buffer
	free(engine->response_headers);

	free(engine);
	return 0;
}

// ============================================================
// Context Methods
// ============================================================

/** Helper to pop Ctx struct from stack */
static http_ctx_t* pop_ctx(qd_context* ctx) {
	qd_stack_element_t ctx_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &ctx_elem);
	if (err != QD_STACK_OK || ctx_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error: expected Ctx struct\n");
		abort();
	}
	return (http_ctx_t*)ctx_elem.value.p;
}

int usr_http_param(qd_context* ctx) {
	// Pop name
	qd_stack_element_t name_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &name_elem);
	if (err != QD_STACK_OK || name_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::param: expected name string\n");
		abort();
	}

	// Pop ctx
	http_ctx_t* http_ctx = pop_ctx(ctx);

	char value[4096];
	find_param(qd_string_data(http_ctx->params), qd_string_data(name_elem.value.s), value, sizeof(value));

	qd_string_release(name_elem.value.s);
	qd_push_s(ctx, value);
	return 0;
}

int usr_http_query_param(qd_context* ctx) {
	// Pop name
	qd_stack_element_t name_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &name_elem);
	if (err != QD_STACK_OK || name_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::query_param: expected name string\n");
		abort();
	}

	// Pop ctx
	http_ctx_t* http_ctx = pop_ctx(ctx);

	char value[4096];
	find_query_param(qd_string_data(http_ctx->query), qd_string_data(name_elem.value.s), value, sizeof(value));

	qd_string_release(name_elem.value.s);
	qd_push_s(ctx, value);
	return 0;
}

int usr_http_get_header(qd_context* ctx) {
	// Pop name
	qd_stack_element_t name_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &name_elem);
	if (err != QD_STACK_OK || name_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::get_header: expected name string\n");
		abort();
	}

	// Pop ctx
	http_ctx_t* http_ctx = pop_ctx(ctx);

	char value[4096];
	find_header(qd_string_data(http_ctx->headers), qd_string_data(name_elem.value.s), value, sizeof(value));

	qd_string_release(name_elem.value.s);
	qd_push_s(ctx, value);
	return 0;
}

/** Internal: Send response with content type */
static int send_response_internal(qd_context* ctx, const char* content_type) {
	// Pop body
	qd_stack_element_t body_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &body_elem);
	if (err != QD_STACK_OK || body_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http response: expected body string\n");
		abort();
	}

	// Pop status
	qd_stack_element_t status_elem;
	err = qd_stack_pop(ctx->st, &status_elem);
	if (err != QD_STACK_OK || status_elem.type != QD_STACK_TYPE_INT) {
		qd_string_release(body_elem.value.s);
		fprintf(stderr, "Fatal error in http response: expected status integer\n");
		abort();
	}

	// Pop ctx
	http_ctx_t* http_ctx = pop_ctx(ctx);

	if (!http_ctx->responded) {
		send_response((int)http_ctx->socket, (int)status_elem.value.i, http_ctx->response_headers, content_type,
				qd_string_data(body_elem.value.s));
		http_ctx->responded = 1;
	}

	qd_string_release(body_elem.value.s);
	return 0;
}

int usr_http_string(qd_context* ctx) {
	return send_response_internal(ctx, "text/plain; charset=utf-8");
}

int usr_http_json(qd_context* ctx) {
	return send_response_internal(ctx, "application/json; charset=utf-8");
}

int usr_http_html(qd_context* ctx) {
	return send_response_internal(ctx, "text/html; charset=utf-8");
}

int usr_http_set_header(qd_context* ctx) {
	// Pop value
	qd_stack_element_t value_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &value_elem);
	if (err != QD_STACK_OK || value_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::set_header: expected value string\n");
		abort();
	}

	// Pop name
	qd_stack_element_t name_elem;
	err = qd_stack_pop(ctx->st, &name_elem);
	if (err != QD_STACK_OK || name_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(value_elem.value.s);
		fprintf(stderr, "Fatal error in http::set_header: expected name string\n");
		abort();
	}

	// Pop ctx
	http_ctx_t* http_ctx = pop_ctx(ctx);

	// Store header in response_headers buffer
	const char* name = qd_string_data(name_elem.value.s);
	const char* value = qd_string_data(value_elem.value.s);
	size_t name_len = strlen(name);
	size_t value_len = strlen(value);
	size_t needed = name_len + 2 + value_len + 2; // "Name: Value\r\n"

	// Grow buffer if needed
	if (http_ctx->response_headers_len + needed + 1 > http_ctx->response_headers_cap) {
		size_t new_cap = http_ctx->response_headers_cap == 0 ? 512 : http_ctx->response_headers_cap * 2;
		while (new_cap < http_ctx->response_headers_len + needed + 1) {
			new_cap *= 2;
		}
		char* new_buf = realloc(http_ctx->response_headers, new_cap);
		if (!new_buf) {
			qd_string_release(name_elem.value.s);
			qd_string_release(value_elem.value.s);
			fprintf(stderr, "Fatal error in http::set_header: memory allocation failed\n");
			abort();
		}
		http_ctx->response_headers = new_buf;
		http_ctx->response_headers_cap = new_cap;
	}

	// Append header
	char* p = http_ctx->response_headers + http_ctx->response_headers_len;
	memcpy(p, name, name_len);
	p += name_len;
	*p++ = ':';
	*p++ = ' ';
	memcpy(p, value, value_len);
	p += value_len;
	*p++ = '\r';
	*p++ = '\n';
	*p = '\0';
	http_ctx->response_headers_len += needed;

	qd_string_release(name_elem.value.s);
	qd_string_release(value_elem.value.s);
	return 0;
}

int usr_http_abort(qd_context* ctx) {
	return send_response_internal(ctx, "text/plain; charset=utf-8");
}

int usr_http_is_responded(qd_context* ctx) {
	// Pop ctx
	http_ctx_t* http_ctx = pop_ctx(ctx);
	qd_push_i(ctx, http_ctx->responded);
	return 0;
}

// ============================================================
// Route Groups
// ============================================================

int usr_http_group(qd_context* ctx) {
	// Pop prefix
	qd_stack_element_t prefix_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &prefix_elem);
	if (err != QD_STACK_OK || prefix_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::group: expected prefix string\n");
		abort();
	}

	// Pop engine
	qd_stack_element_t engine_elem;
	err = qd_stack_pop(ctx->st, &engine_elem);
	if (err != QD_STACK_OK || engine_elem.type != QD_STACK_TYPE_PTR) {
		qd_string_release(prefix_elem.value.s);
		fprintf(stderr, "Fatal error in http::group: expected engine pointer\n");
		abort();
	}

	http_engine_t* engine = (http_engine_t*)engine_elem.value.p;

	if (engine->group_count >= HTTP_MAX_GROUPS) {
		qd_string_release(prefix_elem.value.s);
		fprintf(stderr, "Fatal error: too many route groups\n");
		abort();
	}

	int group_idx = engine->group_count++;
	http_group_t* group = &engine->groups[group_idx];
	group->prefix = strdup(qd_string_data(prefix_elem.value.s));
	if (!group->prefix) {
		engine->group_count--; // Rollback
		qd_string_release(prefix_elem.value.s);
		fprintf(stderr, "Fatal error in http::group: memory allocation failed\n");
		abort();
	}
	group->middleware_count = 0;

	qd_string_release(prefix_elem.value.s);

	// Return group as (engine_ptr, group_idx) packed into a single pointer
	// We'll use a small struct for this
	int64_t* group_handle = malloc(sizeof(int64_t) * 2);
	if (!group_handle) {
		free(group->prefix);
		engine->group_count--; // Rollback
		fprintf(stderr, "Fatal error in http::group: memory allocation failed\n");
		abort();
	}
	group_handle[0] = (int64_t)(uintptr_t)engine;
	group_handle[1] = group_idx;

	// Track handle for cleanup
	engine->group_handles[engine->group_handle_count++] = group_handle;

	qd_push_p(ctx, group_handle);
	return 0;
}

/** Helper to unpack group handle */
static void unpack_group_handle(void* handle, http_engine_t** engine, int* group_idx) {
	int64_t* h = (int64_t*)handle;
	*engine = (http_engine_t*)(uintptr_t)h[0];
	*group_idx = (int)h[1];
}

int usr_http_group_use(qd_context* ctx) {
	// Pop middleware
	qd_stack_element_t mw_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &mw_elem);
	if (err != QD_STACK_OK || mw_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in http::group_use: expected middleware function\n");
		abort();
	}

	// Pop group handle
	qd_stack_element_t group_elem;
	err = qd_stack_pop(ctx->st, &group_elem);
	if (err != QD_STACK_OK || group_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in http::group_use: expected group pointer\n");
		abort();
	}

	http_engine_t* engine;
	int group_idx;
	unpack_group_handle(group_elem.value.p, &engine, &group_idx);

	http_group_t* group = &engine->groups[group_idx];
	if (group->middleware_count >= HTTP_MAX_MIDDLEWARE) {
		fprintf(stderr, "Fatal error: too many middlewares in group\n");
		abort();
	}

	group->middlewares[group->middleware_count++] = mw_elem.value.p;
	return 0;
}

/** Internal: Register route on group */
static int register_group_route(qd_context* ctx, http_method_t method) {
	// Pop handler
	qd_stack_element_t handler_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &handler_elem);
	if (err != QD_STACK_OK || handler_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in http group route registration: expected handler function\n");
		abort();
	}

	// Pop path
	qd_stack_element_t path_elem;
	err = qd_stack_pop(ctx->st, &path_elem);
	if (err != QD_STACK_OK || path_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http group route registration: expected path string\n");
		abort();
	}

	// Pop group handle
	qd_stack_element_t group_elem;
	err = qd_stack_pop(ctx->st, &group_elem);
	if (err != QD_STACK_OK || group_elem.type != QD_STACK_TYPE_PTR) {
		qd_string_release(path_elem.value.s);
		fprintf(stderr, "Fatal error in http group route registration: expected group pointer\n");
		abort();
	}

	http_engine_t* engine;
	int group_idx;
	unpack_group_handle(group_elem.value.p, &engine, &group_idx);

	if (engine->route_count >= HTTP_MAX_ROUTES) {
		qd_string_release(path_elem.value.s);
		fprintf(stderr, "Fatal error: too many routes\n");
		abort();
	}

	http_route_t* route = &engine->routes[engine->route_count++];
	route->method = method;
	route->path = strdup(qd_string_data(path_elem.value.s));
	if (!route->path) {
		engine->route_count--; // Rollback
		qd_string_release(path_elem.value.s);
		fprintf(stderr, "Fatal error in http group route registration: memory allocation failed\n");
		abort();
	}
	route->handler = handler_elem.value.p;
	route->group_idx = group_idx;

	qd_string_release(path_elem.value.s);
	return 0;
}

int usr_http_group_GET(qd_context* ctx) {
	return register_group_route(ctx, HTTP_METHOD_GET);
}

int usr_http_group_POST(qd_context* ctx) {
	return register_group_route(ctx, HTTP_METHOD_POST);
}

int usr_http_group_PUT(qd_context* ctx) {
	return register_group_route(ctx, HTTP_METHOD_PUT);
}

int usr_http_group_DELETE(qd_context* ctx) {
	return register_group_route(ctx, HTTP_METHOD_DELETE);
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

/** Send file response (internal helper) */
static int send_file_response(int socket, const char* filepath, const char* extra_headers) {
	// Open file
	int fd = open(filepath, O_RDONLY);
	if (fd < 0) {
		return -1; // File not found or permission denied
	}

	// Get file size
	off_t file_size = lseek(fd, 0, SEEK_END);
	if (file_size < 0) {
		close(fd);
		return -1;
	}
	lseek(fd, 0, SEEK_SET);

	// Read file into buffer
	char* file_data = malloc((size_t)file_size);
	if (!file_data) {
		close(fd);
		return -2; // Memory error
	}

	ssize_t bytes_read = read(fd, file_data, (size_t)file_size);
	close(fd);

	if (bytes_read != file_size) {
		free(file_data);
		return -1;
	}

	// Get MIME type
	const char* content_type = get_mime_type(filepath);

	// Build response header
	char header[4096];
	int header_len = snprintf(header, sizeof(header),
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %zu\r\n"
			"Connection: close\r\n"
			"%s"
			"\r\n",
			content_type, (size_t)file_size, extra_headers ? extra_headers : "");

	// Send response
	if (send(socket, header, (size_t)header_len, 0) < 0) {
		free(file_data);
		return -3;
	}
	if (file_size > 0 && send(socket, file_data, (size_t)file_size, 0) < 0) {
		free(file_data);
		return -3;
	}

	free(file_data);
	return 0;
}

int usr_http_static_file(qd_context* ctx) {
	// Pop filepath
	qd_stack_element_t path_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &path_elem);
	if (err != QD_STACK_OK || path_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::static_file: expected path string\n");
		abort();
	}

	// Pop ctx
	http_ctx_t* http_ctx = pop_ctx(ctx);

	if (http_ctx->responded) {
		qd_string_release(path_elem.value.s);
		return 0;
	}

	const char* filepath = qd_string_data(path_elem.value.s);

	// Security: prevent path traversal
	if (strstr(filepath, "..") != NULL) {
		send_response((int)http_ctx->socket, 403, http_ctx->response_headers, "text/plain", "Forbidden");
		http_ctx->responded = 1;
		qd_string_release(path_elem.value.s);
		return 0;
	}

	int result = send_file_response((int)http_ctx->socket, filepath, http_ctx->response_headers);
	if (result < 0) {
		send_response((int)http_ctx->socket, 404, http_ctx->response_headers, "text/plain", "Not Found");
	}
	http_ctx->responded = 1;

	qd_string_release(path_elem.value.s);
	return 0;
}

int usr_http_static(qd_context* ctx) {
	// Pop fs_path (filesystem path)
	qd_stack_element_t fs_path_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &fs_path_elem);
	if (err != QD_STACK_OK || fs_path_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::static: expected fs_path string\n");
		abort();
	}

	// Pop route_prefix
	qd_stack_element_t prefix_elem;
	err = qd_stack_pop(ctx->st, &prefix_elem);
	if (err != QD_STACK_OK || prefix_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(fs_path_elem.value.s);
		fprintf(stderr, "Fatal error in http::static: expected prefix string\n");
		abort();
	}

	// Pop ctx
	http_ctx_t* http_ctx = pop_ctx(ctx);

	if (http_ctx->responded) {
		qd_string_release(fs_path_elem.value.s);
		qd_string_release(prefix_elem.value.s);
		return 0;
	}

	const char* fs_path = qd_string_data(fs_path_elem.value.s);
	const char* prefix = qd_string_data(prefix_elem.value.s);
	const char* request_path = qd_string_data(http_ctx->path);

	// Check if request path starts with prefix
	size_t prefix_len = strlen(prefix);
	if (strncmp(request_path, prefix, prefix_len) != 0) {
		send_response((int)http_ctx->socket, 404, http_ctx->response_headers, "text/plain", "Not Found");
		http_ctx->responded = 1;
		qd_string_release(fs_path_elem.value.s);
		qd_string_release(prefix_elem.value.s);
		return 0;
	}

	// Get the relative path after the prefix
	const char* rel_path = request_path + prefix_len;
	if (*rel_path == '/') {
		rel_path++; // Skip leading slash
	}

	// Security: prevent path traversal
	if (strstr(rel_path, "..") != NULL) {
		send_response((int)http_ctx->socket, 403, http_ctx->response_headers, "text/plain", "Forbidden");
		http_ctx->responded = 1;
		qd_string_release(fs_path_elem.value.s);
		qd_string_release(prefix_elem.value.s);
		return 0;
	}

	// Build full filepath
	char filepath[4096];
	size_t fs_path_len = strlen(fs_path);
	if (fs_path_len > 0 && fs_path[fs_path_len - 1] == '/') {
		snprintf(filepath, sizeof(filepath), "%s%s", fs_path, rel_path);
	} else {
		snprintf(filepath, sizeof(filepath), "%s/%s", fs_path, rel_path);
	}

	// If path is empty or ends with /, try index.html
	size_t filepath_len = strlen(filepath);
	if (filepath_len == 0 || filepath[filepath_len - 1] == '/') {
		strncat(filepath, "index.html", sizeof(filepath) - filepath_len - 1);
	}

	int result = send_file_response((int)http_ctx->socket, filepath, http_ctx->response_headers);
	if (result < 0) {
		send_response((int)http_ctx->socket, 404, http_ctx->response_headers, "text/plain", "Not Found");
	}
	http_ctx->responded = 1;

	qd_string_release(fs_path_elem.value.s);
	qd_string_release(prefix_elem.value.s);
	return 0;
}

// ============================================================
// Server-Sent Events (SSE)
// ============================================================

int usr_http_sse_start(qd_context* ctx) {
	// Pop ctx
	http_ctx_t* http_ctx = pop_ctx(ctx);

	if (http_ctx->responded || http_ctx->sse_mode) {
		return 0; // Already responded or SSE already started
	}

	// Build SSE headers
	char header[4096];
	int header_len = snprintf(header, sizeof(header),
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/event-stream\r\n"
			"Cache-Control: no-cache\r\n"
			"Connection: keep-alive\r\n"
			"%s"
			"\r\n",
			http_ctx->response_headers ? http_ctx->response_headers : "");

	if (send((int)http_ctx->socket, header, (size_t)header_len, 0) < 0) {
		http_ctx->responded = 1;
		return 0;
	}

	http_ctx->sse_mode = 1;
	return 0;
}

int usr_http_sse_send(qd_context* ctx) {
	// Pop data
	qd_stack_element_t data_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &data_elem);
	if (err != QD_STACK_OK || data_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::sse_send: expected data string\n");
		abort();
	}

	// Pop ctx
	http_ctx_t* http_ctx = pop_ctx(ctx);

	if (!http_ctx->sse_mode) {
		qd_string_release(data_elem.value.s);
		return 0; // SSE not started
	}

	const char* data = qd_string_data(data_elem.value.s);
	size_t data_len = strlen(data);

	// Format: "data: <content>\n\n"
	// For multi-line data, each line needs "data: " prefix
	// For simplicity, we send the whole thing as one data line
	char* event = malloc(6 + data_len + 2 + 1); // "data: " + data + "\n\n" + null
	if (!event) {
		qd_string_release(data_elem.value.s);
		fprintf(stderr, "Fatal error in http::sse_send: memory allocation failed\n");
		abort();
	}

	memcpy(event, "data: ", 6);
	memcpy(event + 6, data, data_len);
	event[6 + data_len] = '\n';
	event[6 + data_len + 1] = '\n';
	event[6 + data_len + 2] = '\0';

	ssize_t sent = send((int)http_ctx->socket, event, 6 + data_len + 2, 0);
	free(event);

	if (sent < 0) {
		// Connection closed by client
		http_ctx->sse_mode = 0;
		http_ctx->responded = 1;
	}

	qd_string_release(data_elem.value.s);
	return 0;
}

int usr_http_sse_event(qd_context* ctx) {
	// Pop data
	qd_stack_element_t data_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &data_elem);
	if (err != QD_STACK_OK || data_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in http::sse_event: expected data string\n");
		abort();
	}

	// Pop event name
	qd_stack_element_t name_elem;
	err = qd_stack_pop(ctx->st, &name_elem);
	if (err != QD_STACK_OK || name_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(data_elem.value.s);
		fprintf(stderr, "Fatal error in http::sse_event: expected event name string\n");
		abort();
	}

	// Pop ctx
	http_ctx_t* http_ctx = pop_ctx(ctx);

	if (!http_ctx->sse_mode) {
		qd_string_release(data_elem.value.s);
		qd_string_release(name_elem.value.s);
		return 0; // SSE not started
	}

	const char* name = qd_string_data(name_elem.value.s);
	const char* data = qd_string_data(data_elem.value.s);
	size_t name_len = strlen(name);
	size_t data_len = strlen(data);

	// Format: "event: <name>\ndata: <content>\n\n"
	size_t event_size = 7 + name_len + 1 + 6 + data_len + 2 + 1;
	char* event = malloc(event_size);
	if (!event) {
		qd_string_release(data_elem.value.s);
		qd_string_release(name_elem.value.s);
		fprintf(stderr, "Fatal error in http::sse_event: memory allocation failed\n");
		abort();
	}

	char* p = event;
	memcpy(p, "event: ", 7);
	p += 7;
	memcpy(p, name, name_len);
	p += name_len;
	*p++ = '\n';
	memcpy(p, "data: ", 6);
	p += 6;
	memcpy(p, data, data_len);
	p += data_len;
	*p++ = '\n';
	*p++ = '\n';
	*p = '\0';

	ssize_t sent = send((int)http_ctx->socket, event, (size_t)(p - event), 0);
	free(event);

	if (sent < 0) {
		http_ctx->sse_mode = 0;
		http_ctx->responded = 1;
	}

	qd_string_release(data_elem.value.s);
	qd_string_release(name_elem.value.s);
	return 0;
}

int usr_http_sse_end(qd_context* ctx) {
	// Pop ctx
	http_ctx_t* http_ctx = pop_ctx(ctx);

	if (http_ctx->sse_mode) {
		http_ctx->sse_mode = 0;
		http_ctx->responded = 1;
	}

	return 0;
}
