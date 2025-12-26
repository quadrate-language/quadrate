/**
 * @file test_http.c
 * @brief Unit tests for the qdhttp HTTP client library
 *
 * Tests HTTP client operations including request creation, response parsing,
 * and memory management.
 */

#include <qdhttp/http.h>
#include <qdrt/runtime.h>
#include <qdrt/context.h>
#include <qdrt/stack.h>
#include <qdrt/qd_string.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <string.h>

// Helper function to create a context
static qd_context* create_test_context(void) {
	qd_context* ctx = qd_create_context(256);
	return ctx;
}

// Helper function to destroy a context
static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}

// ========== Request Creation Tests ==========

TEST(HttpNewBasicTest) {
	qd_context* ctx = create_test_context();

	// Push URL
	qd_push_s(ctx, "http://example.com");

	// Create request
	qd_exec_result result = usr_http_new(ctx);
	ASSERT_EQ(result.code, 0, "http_new should succeed");

	// Check that a request pointer was returned
	qd_stack_element_t req_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &req_elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(req_elem.type, QD_STACK_TYPE_PTR, "request should be ptr");
	ASSERT(req_elem.value.p != NULL, "request should not be NULL");

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

TEST(HttpNewHttpsUrlTest) {
	qd_context* ctx = create_test_context();

	// Push HTTPS URL
	qd_push_s(ctx, "https://example.com/path");

	// Create request
	qd_exec_result result = usr_http_new(ctx);
	ASSERT_EQ(result.code, 0, "http_new with https should succeed");

	// Get request pointer
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	ASSERT(req_elem.value.p != NULL, "request should not be NULL");

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

TEST(HttpNewWithPortTest) {
	qd_context* ctx = create_test_context();

	// Push URL with explicit port
	qd_push_s(ctx, "http://example.com:8080/api");

	// Create request
	qd_exec_result result = usr_http_new(ctx);
	ASSERT_EQ(result.code, 0, "http_new with port should succeed");

	// Get request pointer
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	ASSERT(req_elem.value.p != NULL, "request should not be NULL");

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

TEST(HttpNewWithQueryStringTest) {
	qd_context* ctx = create_test_context();

	// Push URL with query string
	qd_push_s(ctx, "http://example.com/search?q=test&page=1");

	// Create request
	qd_exec_result result = usr_http_new(ctx);
	ASSERT_EQ(result.code, 0, "http_new with query should succeed");

	// Get request pointer
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	ASSERT(req_elem.value.p != NULL, "request should not be NULL");

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

// ========== Method Tests ==========

TEST(HttpMethodPostTest) {
	qd_context* ctx = create_test_context();

	// Create request
	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);

	// Set method to POST
	qd_push_p(ctx, req_elem.value.p);
	qd_push_s(ctx, "POST");
	qd_exec_result result = usr_http_method(ctx);
	ASSERT_EQ(result.code, 0, "http_method should succeed");

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

TEST(HttpMethodPutTest) {
	qd_context* ctx = create_test_context();

	// Create request
	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);

	// Set method to PUT
	qd_push_p(ctx, req_elem.value.p);
	qd_push_s(ctx, "PUT");
	qd_exec_result result = usr_http_method(ctx);
	ASSERT_EQ(result.code, 0, "http_method PUT should succeed");

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

TEST(HttpMethodDeleteTest) {
	qd_context* ctx = create_test_context();

	// Create request
	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);

	// Set method to DELETE
	qd_push_p(ctx, req_elem.value.p);
	qd_push_s(ctx, "DELETE");
	qd_exec_result result = usr_http_method(ctx);
	ASSERT_EQ(result.code, 0, "http_method DELETE should succeed");

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

// ========== Header Tests ==========

TEST(HttpHeaderBasicTest) {
	qd_context* ctx = create_test_context();

	// Create request
	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);

	// Add header
	qd_push_p(ctx, req_elem.value.p);
	qd_push_s(ctx, "Content-Type");
	qd_push_s(ctx, "application/json");
	qd_exec_result result = usr_http_header(ctx);
	ASSERT_EQ(result.code, 0, "http_header should succeed");

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

TEST(HttpMultipleHeadersTest) {
	qd_context* ctx = create_test_context();

	// Create request
	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);

	// Add multiple headers
	qd_push_p(ctx, req_elem.value.p);
	qd_push_s(ctx, "Content-Type");
	qd_push_s(ctx, "application/json");
	usr_http_header(ctx);

	qd_push_p(ctx, req_elem.value.p);
	qd_push_s(ctx, "Authorization");
	qd_push_s(ctx, "Bearer token123");
	usr_http_header(ctx);

	qd_push_p(ctx, req_elem.value.p);
	qd_push_s(ctx, "Accept");
	qd_push_s(ctx, "application/json");
	qd_exec_result result = usr_http_header(ctx);
	ASSERT_EQ(result.code, 0, "multiple headers should succeed");

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

// ========== Body Tests ==========

TEST(HttpBodyBasicTest) {
	qd_context* ctx = create_test_context();

	// Create request
	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);

	// Set body
	qd_push_p(ctx, req_elem.value.p);
	qd_push_s(ctx, "{\"key\":\"value\"}");
	qd_exec_result result = usr_http_body(ctx);
	ASSERT_EQ(result.code, 0, "http_body should succeed");

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

TEST(HttpBodyEmptyTest) {
	qd_context* ctx = create_test_context();

	// Create request
	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);

	// Set empty body
	qd_push_p(ctx, req_elem.value.p);
	qd_push_s(ctx, "");
	qd_exec_result result = usr_http_body(ctx);
	ASSERT_EQ(result.code, 0, "empty body should succeed");

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

// ========== Integration Tests (require network) ==========

TEST(HttpSendGetTest) {
	qd_context* ctx = create_test_context();

	// Create request to example.com (real network call)
	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);

	// Send request
	qd_push_p(ctx, req_elem.value.p);
	qd_exec_result result = usr_http_send(ctx);
	ASSERT_EQ(result.code, 0, "http_send should succeed");

	// Pop status code (Ok=1)
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ(status_elem.type, QD_STACK_TYPE_INT, "status should be int");
	ASSERT_EQ((int)status_elem.value.i, 1, "status should be Ok (1)");

	// Pop response pointer
	qd_stack_element_t resp_elem;
	qd_stack_pop(ctx->st, &resp_elem);
	ASSERT_EQ(resp_elem.type, QD_STACK_TYPE_PTR, "response should be ptr");
	ASSERT(resp_elem.value.p != NULL, "response should not be NULL");

	// Close response
	qd_push_p(ctx, resp_elem.value.p);
	usr_http_close(ctx);

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

TEST(HttpSendHttpsTest) {
	qd_context* ctx = create_test_context();

	// Create HTTPS request
	qd_push_s(ctx, "https://example.com");
	usr_http_new(ctx);
	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);

	// Send request
	qd_push_p(ctx, req_elem.value.p);
	qd_exec_result result = usr_http_send(ctx);
	ASSERT_EQ(result.code, 0, "https send should succeed");

	// Pop status code
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ((int)status_elem.value.i, 1, "status should be Ok (1)");

	// Pop response pointer
	qd_stack_element_t resp_elem;
	qd_stack_pop(ctx->st, &resp_elem);
	ASSERT(resp_elem.value.p != NULL, "response should not be NULL");

	// Close response
	qd_push_p(ctx, resp_elem.value.p);
	usr_http_close(ctx);

	// Free request
	qd_push_p(ctx, req_elem.value.p);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

TEST(HttpGetSimpleTest) {
	qd_context* ctx = create_test_context();

	// Simple GET request
	qd_push_s(ctx, "http://example.com");
	qd_exec_result result = usr_http_get(ctx);
	ASSERT_EQ(result.code, 0, "http_get should succeed");

	// Pop status code
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ((int)status_elem.value.i, 1, "status should be Ok (1)");

	// Pop response pointer
	qd_stack_element_t resp_elem;
	qd_stack_pop(ctx->st, &resp_elem);
	ASSERT(resp_elem.value.p != NULL, "response should not be NULL");

	// Close response
	qd_push_p(ctx, resp_elem.value.p);
	usr_http_close(ctx);

	destroy_test_context(ctx);
}

// ========== Memory Management Tests ==========

TEST(HttpRequestFreeTest) {
	qd_context* ctx = create_test_context();

	// Create and free multiple requests (check for leaks with valgrind)
	for (int i = 0; i < 10; i++) {
		qd_push_s(ctx, "http://example.com/test");
		usr_http_new(ctx);
		qd_stack_element_t req_elem;
		qd_stack_pop(ctx->st, &req_elem);

		// Add headers and body
		qd_push_p(ctx, req_elem.value.p);
		qd_push_s(ctx, "Content-Type");
		qd_push_s(ctx, "text/plain");
		usr_http_header(ctx);

		qd_push_p(ctx, req_elem.value.p);
		qd_push_s(ctx, "test body content");
		usr_http_body(ctx);

		// Free request
		qd_push_p(ctx, req_elem.value.p);
		usr_http_free_request(ctx);
	}

	destroy_test_context(ctx);
}

// ========== Engine Tests ==========

TEST(HttpEngineCreateTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	qd_exec_result result = usr_http_engine(ctx);
	ASSERT_EQ(result.code, 0, "http_engine should succeed");

	// Pop engine pointer
	qd_stack_element_t engine_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &engine_elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(engine_elem.type, QD_STACK_TYPE_PTR, "engine should be ptr");
	ASSERT(engine_elem.value.p != NULL, "engine should not be NULL");

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

TEST(HttpEngineGroupTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Create group
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/api");
	qd_exec_result result = usr_http_group(ctx);
	ASSERT_EQ(result.code, 0, "http_group should succeed");

	// Pop group pointer
	qd_stack_element_t group_elem;
	qd_stack_pop(ctx->st, &group_elem);
	ASSERT(group_elem.value.p != NULL, "group should not be NULL");

	// Free engine (groups are freed with engine)
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

// ========== Server Route Registration Tests ==========

// Dummy handler for route tests
static void dummy_handler(qd_context* ctx) {
	(void)ctx;
}

TEST(HttpRouteGetTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Register GET route
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/users");
	qd_push_p(ctx, (void*)dummy_handler);
	qd_exec_result result = usr_http_GET(ctx);
	ASSERT_EQ(result.code, 0, "http_GET should succeed");

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

TEST(HttpRoutePostTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Register POST route
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/users");
	qd_push_p(ctx, (void*)dummy_handler);
	qd_exec_result result = usr_http_POST(ctx);
	ASSERT_EQ(result.code, 0, "http_POST should succeed");

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

TEST(HttpRoutePutTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Register PUT route
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/users/:id");
	qd_push_p(ctx, (void*)dummy_handler);
	qd_exec_result result = usr_http_PUT(ctx);
	ASSERT_EQ(result.code, 0, "http_PUT should succeed");

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

TEST(HttpRouteDeleteTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Register DELETE route
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/users/:id");
	qd_push_p(ctx, (void*)dummy_handler);
	qd_exec_result result = usr_http_DELETE(ctx);
	ASSERT_EQ(result.code, 0, "http_DELETE should succeed");

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

TEST(HttpRouteAnyTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Register ANY route (matches all methods)
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/health");
	qd_push_p(ctx, (void*)dummy_handler);
	qd_exec_result result = usr_http_ANY(ctx);
	ASSERT_EQ(result.code, 0, "http_ANY should succeed");

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

TEST(HttpMultipleRoutesTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Register multiple routes
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/users");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_GET(ctx);

	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/users");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_POST(ctx);

	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/users/:id");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_GET(ctx);

	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/users/:id");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_PUT(ctx);

	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/users/:id");
	qd_push_p(ctx, (void*)dummy_handler);
	qd_exec_result result = usr_http_DELETE(ctx);
	ASSERT_EQ(result.code, 0, "multiple routes should succeed");

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

// ========== Middleware Tests ==========

static void dummy_middleware(qd_context* ctx) {
	(void)ctx;
}

TEST(HttpMiddlewareTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Add middleware
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_p(ctx, (void*)dummy_middleware);
	qd_exec_result result = usr_http_use(ctx);
	ASSERT_EQ(result.code, 0, "http_use should succeed");

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

TEST(HttpMultipleMiddlewareTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Add multiple middleware
	for (int i = 0; i < 5; i++) {
		qd_push_p(ctx, engine_elem.value.p);
		qd_push_p(ctx, (void*)dummy_middleware);
		qd_exec_result result = usr_http_use(ctx);
		ASSERT_EQ(result.code, 0, "http_use should succeed");
	}

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

// ========== Route Group Tests ==========

TEST(HttpGroupRoutesTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Create group
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/api/v1");
	usr_http_group(ctx);
	qd_stack_element_t group_elem;
	qd_stack_pop(ctx->st, &group_elem);

	// Register routes on group
	qd_push_p(ctx, group_elem.value.p);
	qd_push_s(ctx, "/users");
	qd_push_p(ctx, (void*)dummy_handler);
	qd_exec_result result = usr_http_group_GET(ctx);
	ASSERT_EQ(result.code, 0, "group_GET should succeed");

	qd_push_p(ctx, group_elem.value.p);
	qd_push_s(ctx, "/users");
	qd_push_p(ctx, (void*)dummy_handler);
	result = usr_http_group_POST(ctx);
	ASSERT_EQ(result.code, 0, "group_POST should succeed");

	qd_push_p(ctx, group_elem.value.p);
	qd_push_s(ctx, "/users/:id");
	qd_push_p(ctx, (void*)dummy_handler);
	result = usr_http_group_PUT(ctx);
	ASSERT_EQ(result.code, 0, "group_PUT should succeed");

	qd_push_p(ctx, group_elem.value.p);
	qd_push_s(ctx, "/users/:id");
	qd_push_p(ctx, (void*)dummy_handler);
	result = usr_http_group_DELETE(ctx);
	ASSERT_EQ(result.code, 0, "group_DELETE should succeed");

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

TEST(HttpGroupMiddlewareTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Create group
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/admin");
	usr_http_group(ctx);
	qd_stack_element_t group_elem;
	qd_stack_pop(ctx->st, &group_elem);

	// Add middleware to group
	qd_push_p(ctx, group_elem.value.p);
	qd_push_p(ctx, (void*)dummy_middleware);
	qd_exec_result result = usr_http_group_use(ctx);
	ASSERT_EQ(result.code, 0, "group_use should succeed");

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

TEST(HttpMultipleGroupsTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Create multiple groups
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/api/v1");
	usr_http_group(ctx);
	qd_stack_element_t group1_elem;
	qd_stack_pop(ctx->st, &group1_elem);

	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/api/v2");
	usr_http_group(ctx);
	qd_stack_element_t group2_elem;
	qd_stack_pop(ctx->st, &group2_elem);

	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/admin");
	usr_http_group(ctx);
	qd_stack_element_t group3_elem;
	qd_stack_pop(ctx->st, &group3_elem);

	// Add routes to each group
	qd_push_p(ctx, group1_elem.value.p);
	qd_push_s(ctx, "/users");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_group_GET(ctx);

	qd_push_p(ctx, group2_elem.value.p);
	qd_push_s(ctx, "/users");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_group_GET(ctx);

	qd_push_p(ctx, group3_elem.value.p);
	qd_push_s(ctx, "/settings");
	qd_push_p(ctx, (void*)dummy_handler);
	qd_exec_result result = usr_http_group_GET(ctx);
	ASSERT_EQ(result.code, 0, "multiple groups should succeed");

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

// ========== Server Memory Management Tests ==========

TEST(HttpEngineMemoryTest) {
	qd_context* ctx = create_test_context();

	// Create and free multiple engines
	for (int i = 0; i < 5; i++) {
		usr_http_engine(ctx);
		qd_stack_element_t engine_elem;
		qd_stack_pop(ctx->st, &engine_elem);

		// Add routes
		qd_push_p(ctx, engine_elem.value.p);
		qd_push_s(ctx, "/test");
		qd_push_p(ctx, (void*)dummy_handler);
		usr_http_GET(ctx);

		// Create group
		qd_push_p(ctx, engine_elem.value.p);
		qd_push_s(ctx, "/api");
		usr_http_group(ctx);
		qd_stack_element_t group_elem;
		qd_stack_pop(ctx->st, &group_elem);

		// Add group routes
		qd_push_p(ctx, group_elem.value.p);
		qd_push_s(ctx, "/items");
		qd_push_p(ctx, (void*)dummy_handler);
		usr_http_group_GET(ctx);

		// Free engine
		qd_push_p(ctx, engine_elem.value.p);
		usr_http_free_engine(ctx);
	}

	destroy_test_context(ctx);
}

TEST(HttpComplexSetupTest) {
	qd_context* ctx = create_test_context();

	// Create engine
	usr_http_engine(ctx);
	qd_stack_element_t engine_elem;
	qd_stack_pop(ctx->st, &engine_elem);

	// Add global middleware
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_p(ctx, (void*)dummy_middleware);
	usr_http_use(ctx);

	// Add root routes
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/health");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_GET(ctx);

	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_GET(ctx);

	// Create API v1 group
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/api/v1");
	usr_http_group(ctx);
	qd_stack_element_t api_group;
	qd_stack_pop(ctx->st, &api_group);

	// Add auth middleware to API group
	qd_push_p(ctx, api_group.value.p);
	qd_push_p(ctx, (void*)dummy_middleware);
	usr_http_group_use(ctx);

	// Add API routes
	qd_push_p(ctx, api_group.value.p);
	qd_push_s(ctx, "/users");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_group_GET(ctx);

	qd_push_p(ctx, api_group.value.p);
	qd_push_s(ctx, "/users");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_group_POST(ctx);

	qd_push_p(ctx, api_group.value.p);
	qd_push_s(ctx, "/users/:id");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_group_GET(ctx);

	qd_push_p(ctx, api_group.value.p);
	qd_push_s(ctx, "/users/:id");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_group_PUT(ctx);

	qd_push_p(ctx, api_group.value.p);
	qd_push_s(ctx, "/users/:id");
	qd_push_p(ctx, (void*)dummy_handler);
	usr_http_group_DELETE(ctx);

	// Create admin group
	qd_push_p(ctx, engine_elem.value.p);
	qd_push_s(ctx, "/admin");
	usr_http_group(ctx);
	qd_stack_element_t admin_group;
	qd_stack_pop(ctx->st, &admin_group);

	// Add admin middleware
	qd_push_p(ctx, admin_group.value.p);
	qd_push_p(ctx, (void*)dummy_middleware);
	usr_http_group_use(ctx);

	// Add admin routes
	qd_push_p(ctx, admin_group.value.p);
	qd_push_s(ctx, "/dashboard");
	qd_push_p(ctx, (void*)dummy_handler);
	qd_exec_result result = usr_http_group_GET(ctx);
	ASSERT_EQ(result.code, 0, "complex setup should succeed");

	// Free engine
	qd_push_p(ctx, engine_elem.value.p);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

// Main - required for test executable
int main(void) {
	return UC_PrintResults();
}
