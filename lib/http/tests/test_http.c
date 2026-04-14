/**
 * @file test_http.c
 * @brief Unit tests for the qdhttp HTTP module
 *
 * Tests cover creation/destruction of requests and engines, request
 * manipulation (method, headers, body, cert), and route/group
 * registration -- everything that can be exercised without a live
 * network connection.
 *
 * Functions that inherently require networking (usr_http_send,
 * usr_http_get, usr_http_run, usr_http_handle_one, usr_http_stop,
 * usr_http_close, and the context/response methods) are not covered
 * here; they need integration tests with a real or loopback server.
 */

#include <quadrate/http/http.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static qd_context* create_test_context(void) {
	return qd_create_context(256);
}

static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}

/*
 * We need to push a "handler" pointer when registering routes.  The
 * engine stores it as void* and the tests never actually invoke it,
 * so we just use an arbitrary non-NULL data pointer.  This avoids the
 * ISO C warning about converting a function pointer to void*.
 */
static int dummy_handler_storage;
static void* const dummy_handler = &dummy_handler_storage;

/* ================================================================== */
/* Client-side request tests                                          */
/* ================================================================== */

/* ------------------------------------------------------------------ */
/* Test 1: Create a new request and free it                           */
/* ------------------------------------------------------------------ */

TEST(HttpNewAndFreeRequestTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);

	/* Stack should contain the request pointer. */
	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "stack should have 1 element after new");

	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	ASSERT_EQ(QD_STACK_TYPE_PTR, req_elem.type, "new should push a pointer");
	ASSERT_TRUE(req_elem.value.p != NULL, "request pointer should be non-NULL");

	/* Free via the public API (push ptr, call free_request). */
	qd_stack_push_ptr(ctx->st, req_elem.value.p);
	usr_http_free_request(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after free_request");

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 2: Set the request method                                     */
/* ------------------------------------------------------------------ */

TEST(HttpSetMethodTest) {
	qd_context* ctx = create_test_context();

	/* Create request. */
	qd_push_s(ctx, "http://example.com/api");
	usr_http_new(ctx);

	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	void* req = req_elem.value.p;

	/* Set method: stack order is ( req method -- ) */
	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "POST");
	usr_http_method(ctx);

	/* Method call consumes both args; stack should be empty. */
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after method");

	/* Clean up. */
	qd_stack_push_ptr(ctx->st, req);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 3: Add a single header                                        */
/* ------------------------------------------------------------------ */

TEST(HttpAddHeaderTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);

	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	void* req = req_elem.value.p;

	/* Stack order: ( req name value -- ) */
	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "Content-Type");
	qd_push_s(ctx, "application/json");
	usr_http_header(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after header");

	qd_stack_push_ptr(ctx->st, req);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 4: Add multiple headers                                       */
/* ------------------------------------------------------------------ */

TEST(HttpMultipleHeadersTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);

	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	void* req = req_elem.value.p;

	/* Add several headers in sequence. */
	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "Content-Type");
	qd_push_s(ctx, "text/plain");
	usr_http_header(ctx);

	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "Authorization");
	qd_push_s(ctx, "Bearer token123");
	usr_http_header(ctx);

	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "Accept");
	qd_push_s(ctx, "*/*");
	usr_http_header(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after multiple headers");

	qd_stack_push_ptr(ctx->st, req);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 5: Set the request body                                       */
/* ------------------------------------------------------------------ */

TEST(HttpSetBodyTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "http://example.com/post");
	usr_http_new(ctx);

	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	void* req = req_elem.value.p;

	/* Stack order: ( req body -- ) */
	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "{\"key\":\"value\"}");
	usr_http_body(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after body");

	qd_stack_push_ptr(ctx->st, req);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 6: Set client certificate for mTLS                            */
/* ------------------------------------------------------------------ */

TEST(HttpSetCertTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "https://secure.example.com");
	usr_http_new(ctx);

	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	void* req = req_elem.value.p;

	/* Stack order: ( req cert_path key_path -- ) */
	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "/path/to/cert.pem");
	qd_push_s(ctx, "/path/to/key.pem");
	usr_http_cert(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after cert");

	qd_stack_push_ptr(ctx->st, req);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 7: Full request lifecycle -- new, method, header, body, free  */
/* ------------------------------------------------------------------ */

TEST(HttpFullRequestLifecycleTest) {
	qd_context* ctx = create_test_context();

	/* Create */
	qd_push_s(ctx, "http://api.example.com/users");
	usr_http_new(ctx);

	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	void* req = req_elem.value.p;

	/* Method */
	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "PUT");
	usr_http_method(ctx);

	/* Header */
	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "Content-Type");
	qd_push_s(ctx, "application/json");
	usr_http_header(ctx);

	/* Body */
	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "{\"name\":\"test\"}");
	usr_http_body(ctx);

	/* Free */
	qd_stack_push_ptr(ctx->st, req);
	usr_http_free_request(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after full lifecycle");

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 8: Overwrite method twice (no leak / crash)                   */
/* ------------------------------------------------------------------ */

TEST(HttpOverwriteMethodTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);

	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	void* req = req_elem.value.p;

	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "POST");
	usr_http_method(ctx);

	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "DELETE");
	usr_http_method(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack empty after overwriting method");

	qd_stack_push_ptr(ctx->st, req);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 9: Overwrite body twice (no leak / crash)                     */
/* ------------------------------------------------------------------ */

TEST(HttpOverwriteBodyTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "http://example.com");
	usr_http_new(ctx);

	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	void* req = req_elem.value.p;

	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "first body");
	usr_http_body(ctx);

	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "second body");
	usr_http_body(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack empty after overwriting body");

	qd_stack_push_ptr(ctx->st, req);
	usr_http_free_request(ctx);

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 10: Overwrite cert paths twice (no leak / crash)              */
/* ------------------------------------------------------------------ */

TEST(HttpOverwriteCertTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "https://example.com");
	usr_http_new(ctx);

	qd_stack_element_t req_elem;
	qd_stack_pop(ctx->st, &req_elem);
	void* req = req_elem.value.p;

	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "/first/cert.pem");
	qd_push_s(ctx, "/first/key.pem");
	usr_http_cert(ctx);

	qd_stack_push_ptr(ctx->st, req);
	qd_push_s(ctx, "/second/cert.pem");
	qd_push_s(ctx, "/second/key.pem");
	usr_http_cert(ctx);

	qd_stack_push_ptr(ctx->st, req);
	usr_http_free_request(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack empty after overwriting cert");

	destroy_test_context(ctx);
}

/* ================================================================== */
/* Server-side engine tests                                           */
/* ================================================================== */

/* ------------------------------------------------------------------ */
/* Test 11: Create engine and free it                                 */
/* ------------------------------------------------------------------ */

TEST(HttpEngineCreateAndFreeTest) {
	qd_context* ctx = create_test_context();

	usr_http_engine(ctx);

	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "stack should have 1 element after engine");

	qd_stack_element_t eng_elem;
	qd_stack_pop(ctx->st, &eng_elem);
	ASSERT_EQ(QD_STACK_TYPE_PTR, eng_elem.type, "engine should push a pointer");
	ASSERT_TRUE(eng_elem.value.p != NULL, "engine pointer should be non-NULL");

	qd_stack_push_ptr(ctx->st, eng_elem.value.p);
	usr_http_free_engine(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after free_engine");

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 12: Register a GET route on the engine                        */
/* ------------------------------------------------------------------ */

TEST(HttpRegisterGETRouteTest) {
	qd_context* ctx = create_test_context();

	usr_http_engine(ctx);
	qd_stack_element_t eng_elem;
	qd_stack_pop(ctx->st, &eng_elem);
	void* engine = eng_elem.value.p;

	/* Stack: ( engine path handler -- ) */
	qd_stack_push_ptr(ctx->st, engine);
	qd_push_s(ctx, "/users");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_GET(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack empty after GET route");

	qd_stack_push_ptr(ctx->st, engine);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 13: Register POST, PUT, DELETE, ANY routes                    */
/* ------------------------------------------------------------------ */

TEST(HttpRegisterMultipleRouteMethodsTest) {
	qd_context* ctx = create_test_context();

	usr_http_engine(ctx);
	qd_stack_element_t eng_elem;
	qd_stack_pop(ctx->st, &eng_elem);
	void* engine = eng_elem.value.p;

	/* POST */
	qd_stack_push_ptr(ctx->st, engine);
	qd_push_s(ctx, "/items");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_POST(ctx);

	/* PUT */
	qd_stack_push_ptr(ctx->st, engine);
	qd_push_s(ctx, "/items/:id");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_PUT(ctx);

	/* DELETE */
	qd_stack_push_ptr(ctx->st, engine);
	qd_push_s(ctx, "/items/:id");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_DELETE(ctx);

	/* ANY */
	qd_stack_push_ptr(ctx->st, engine);
	qd_push_s(ctx, "/health");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_ANY(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack empty after registering all methods");

	qd_stack_push_ptr(ctx->st, engine);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 14: Add middleware to engine                                   */
/* ------------------------------------------------------------------ */

TEST(HttpUseMiddlewareTest) {
	qd_context* ctx = create_test_context();

	usr_http_engine(ctx);
	qd_stack_element_t eng_elem;
	qd_stack_pop(ctx->st, &eng_elem);
	void* engine = eng_elem.value.p;

	/* Stack: ( engine middleware -- ) */
	qd_stack_push_ptr(ctx->st, engine);
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_use(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack empty after use");

	qd_stack_push_ptr(ctx->st, engine);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 15: Create a route group                                      */
/* ------------------------------------------------------------------ */

TEST(HttpGroupCreateTest) {
	qd_context* ctx = create_test_context();

	usr_http_engine(ctx);
	qd_stack_element_t eng_elem;
	qd_stack_pop(ctx->st, &eng_elem);
	void* engine = eng_elem.value.p;

	/* Stack: ( engine prefix -- group ) */
	qd_stack_push_ptr(ctx->st, engine);
	qd_push_s(ctx, "/api/v1");
	usr_http_group(ctx);

	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "stack should have group pointer");

	qd_stack_element_t grp_elem;
	qd_stack_pop(ctx->st, &grp_elem);
	ASSERT_EQ(QD_STACK_TYPE_PTR, grp_elem.type, "group should be a pointer");
	ASSERT_TRUE(grp_elem.value.p != NULL, "group pointer should be non-NULL");

	/* Free engine (which also frees group handles). */
	qd_stack_push_ptr(ctx->st, engine);
	usr_http_free_engine(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack empty after free_engine with group");

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 16: Register routes on a group                                */
/* ------------------------------------------------------------------ */

TEST(HttpGroupRouteRegistrationTest) {
	qd_context* ctx = create_test_context();

	usr_http_engine(ctx);
	qd_stack_element_t eng_elem;
	qd_stack_pop(ctx->st, &eng_elem);
	void* engine = eng_elem.value.p;

	/* Create group. */
	qd_stack_push_ptr(ctx->st, engine);
	qd_push_s(ctx, "/api");
	usr_http_group(ctx);

	qd_stack_element_t grp_elem;
	qd_stack_pop(ctx->st, &grp_elem);
	void* group = grp_elem.value.p;

	/* Register GET on group: ( group path handler -- ) */
	qd_stack_push_ptr(ctx->st, group);
	qd_push_s(ctx, "/users");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_group_GET(ctx);

	/* Register POST on group. */
	qd_stack_push_ptr(ctx->st, group);
	qd_push_s(ctx, "/users");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_group_POST(ctx);

	/* Register PUT on group. */
	qd_stack_push_ptr(ctx->st, group);
	qd_push_s(ctx, "/users/:id");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_group_PUT(ctx);

	/* Register DELETE on group. */
	qd_stack_push_ptr(ctx->st, group);
	qd_push_s(ctx, "/users/:id");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_group_DELETE(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack empty after group routes");

	qd_stack_push_ptr(ctx->st, engine);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 17: Add middleware to a group                                  */
/* ------------------------------------------------------------------ */

TEST(HttpGroupMiddlewareTest) {
	qd_context* ctx = create_test_context();

	usr_http_engine(ctx);
	qd_stack_element_t eng_elem;
	qd_stack_pop(ctx->st, &eng_elem);
	void* engine = eng_elem.value.p;

	qd_stack_push_ptr(ctx->st, engine);
	qd_push_s(ctx, "/admin");
	usr_http_group(ctx);

	qd_stack_element_t grp_elem;
	qd_stack_pop(ctx->st, &grp_elem);
	void* group = grp_elem.value.p;

	/* Stack: ( group middleware -- ) */
	qd_stack_push_ptr(ctx->st, group);
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_group_use(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack empty after group_use");

	qd_stack_push_ptr(ctx->st, engine);
	usr_http_free_engine(ctx);

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 18: Create/free engine cycle (leak check)                     */
/* ------------------------------------------------------------------ */

TEST(HttpEngineCreateFreeCycleTest) {
	qd_context* ctx = create_test_context();

	for (int i = 0; i < 5; i++) {
		usr_http_engine(ctx);

		qd_stack_element_t eng_elem;
		qd_stack_pop(ctx->st, &eng_elem);

		qd_stack_push_ptr(ctx->st, eng_elem.value.p);
		usr_http_free_engine(ctx);
	}

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack empty after engine create/free cycles");

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 19: Create/free request cycle (leak check)                    */
/* ------------------------------------------------------------------ */

TEST(HttpRequestCreateFreeCycleTest) {
	qd_context* ctx = create_test_context();

	for (int i = 0; i < 10; i++) {
		qd_push_s(ctx, "http://example.com");
		usr_http_new(ctx);

		qd_stack_element_t req_elem;
		qd_stack_pop(ctx->st, &req_elem);

		qd_stack_push_ptr(ctx->st, req_elem.value.p);
		usr_http_free_request(ctx);
	}

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack empty after request create/free cycles");

	destroy_test_context(ctx);
}

/* ------------------------------------------------------------------ */
/* Test 20: Engine with routes and groups -- full lifecycle            */
/* ------------------------------------------------------------------ */

TEST(HttpEngineFullLifecycleTest) {
	qd_context* ctx = create_test_context();

	/* Create engine. */
	usr_http_engine(ctx);
	qd_stack_element_t eng_elem;
	qd_stack_pop(ctx->st, &eng_elem);
	void* engine = eng_elem.value.p;

	/* Add global middleware. */
	qd_stack_push_ptr(ctx->st, engine);
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_use(ctx);

	/* Register root-level routes. */
	qd_stack_push_ptr(ctx->st, engine);
	qd_push_s(ctx, "/");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_GET(ctx);

	qd_stack_push_ptr(ctx->st, engine);
	qd_push_s(ctx, "/health");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_ANY(ctx);

	/* Create a group. */
	qd_stack_push_ptr(ctx->st, engine);
	qd_push_s(ctx, "/api");
	usr_http_group(ctx);

	qd_stack_element_t grp_elem;
	qd_stack_pop(ctx->st, &grp_elem);
	void* group = grp_elem.value.p;

	/* Group middleware. */
	qd_stack_push_ptr(ctx->st, group);
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_group_use(ctx);

	/* Group routes. */
	qd_stack_push_ptr(ctx->st, group);
	qd_push_s(ctx, "/users");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_group_GET(ctx);

	qd_stack_push_ptr(ctx->st, group);
	qd_push_s(ctx, "/users");
	qd_stack_push_ptr(ctx->st, dummy_handler);
	usr_http_group_POST(ctx);

	/* Free everything. */
	qd_stack_push_ptr(ctx->st, engine);
	usr_http_free_engine(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack empty after engine full lifecycle");

	destroy_test_context(ctx);
}

int main(void) {
	return UC_PrintResults();
}
