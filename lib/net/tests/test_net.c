/**
 * @file test_net.c
 * @brief Unit tests for the qdnet network socket library
 */

#include <quadrate/net/net.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <string.h>

// Error codes matching net.c
#define NET_ERR_OK 1

static qd_context* create_test_context(void) {
	return qd_create_context(256);
}

static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 1: DNS lookup of "localhost" -> "127.0.0.1"
// ---------------------------------------------------------------------------

TEST(NetLookupLocalhostTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "localhost");
	usr_net_lookup(ctx);

	// Stack should contain: ip:s result:i
	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(QD_STACK_TYPE_INT, result.type, "lookup result should be int");
	ASSERT_EQ(NET_ERR_OK, (int)result.value.i, "lookup of localhost should succeed");

	qd_stack_element_t ip;
	qd_stack_pop(ctx->st, &ip);
	ASSERT_EQ(QD_STACK_TYPE_STR, ip.type, "lookup ip should be string");
	const char* ip_str = qd_string_data(ip.value.s);
	// localhost may resolve to 127.0.0.1 (IPv4) or ::1 (IPv6)
	int is_loopback = (strcmp(ip_str, "127.0.0.1") == 0 || strcmp(ip_str, "::1") == 0);
	ASSERT_TRUE(is_loopback, "localhost should resolve to loopback address");
	qd_string_release(ip.value.s);

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 2: Listen on port 0 (OS-assigned), verify socket > 0, then close
// ---------------------------------------------------------------------------

TEST(NetListenAndCloseTest) {
	qd_context* ctx = create_test_context();

	// Listen on port 0 -> OS assigns a free port
	qd_push_i(ctx, 0);
	usr_net_listen(ctx);

	// Stack: socket:i result:i
	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(QD_STACK_TYPE_INT, result.type, "listen result should be int");
	ASSERT_EQ(NET_ERR_OK, (int)result.value.i, "listen on port 0 should succeed");

	qd_stack_element_t socket_elem;
	qd_stack_pop(ctx->st, &socket_elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, socket_elem.type, "socket fd should be int");
	ASSERT_TRUE(socket_elem.value.i > 0, "socket fd should be positive");

	// Close the socket
	qd_push_i(ctx, socket_elem.value.i);
	usr_net_close(ctx);

	// Stack should be empty after close
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after close");

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 3: Listen + close cycle doesn't leak (repeated open/close)
// ---------------------------------------------------------------------------

TEST(NetListenCloseCycleTest) {
	qd_context* ctx = create_test_context();

	for (int i = 0; i < 5; i++) {
		qd_push_i(ctx, 0);
		usr_net_listen(ctx);

		qd_stack_element_t result;
		qd_stack_pop(ctx->st, &result);
		ASSERT_EQ(NET_ERR_OK, (int)result.value.i, "listen should succeed in cycle");

		qd_stack_element_t socket_elem;
		qd_stack_pop(ctx->st, &socket_elem);
		ASSERT_TRUE(socket_elem.value.i > 0, "socket fd should be positive in cycle");

		qd_push_i(ctx, socket_elem.value.i);
		usr_net_close(ctx);
	}

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after cycles");

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 4: Connect to non-existent host/port returns error
// ---------------------------------------------------------------------------

TEST(NetConnectFailureTest) {
	qd_context* ctx = create_test_context();

	// Try to connect to a port that nothing is listening on
	qd_push_s(ctx, "127.0.0.1");
	qd_push_i(ctx, 1); // port 1 - very unlikely anything is there
	usr_net_connect(ctx);

	// Should get an error result (not NET_ERR_OK)
	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(QD_STACK_TYPE_INT, result.type, "connect result should be int");
	ASSERT_NE(NET_ERR_OK, (int)result.value.i, "connect to non-existent should fail");

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 5: Set timeout on a listening socket
// ---------------------------------------------------------------------------

TEST(NetSetTimeoutTest) {
	qd_context* ctx = create_test_context();

	// Create a listening socket first
	qd_push_i(ctx, 0);
	usr_net_listen(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(NET_ERR_OK, (int)result.value.i, "listen should succeed for timeout test");

	qd_stack_element_t socket_elem;
	qd_stack_pop(ctx->st, &socket_elem);
	int64_t sock_fd = socket_elem.value.i;

	// Set a 1000ms timeout
	qd_push_i(ctx, sock_fd);
	qd_push_i(ctx, 1000);
	usr_net_set_timeout(ctx);

	qd_stack_element_t timeout_result;
	qd_stack_pop(ctx->st, &timeout_result);
	ASSERT_EQ(QD_STACK_TYPE_INT, timeout_result.type, "timeout result should be int");
	ASSERT_EQ(NET_ERR_OK, (int)timeout_result.value.i, "set_timeout should succeed");

	// Clean up
	qd_push_i(ctx, sock_fd);
	usr_net_close(ctx);

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 6: Set keepalive on a listening socket
// ---------------------------------------------------------------------------

TEST(NetSetKeepaliveTest) {
	qd_context* ctx = create_test_context();

	// Create a listening socket first
	qd_push_i(ctx, 0);
	usr_net_listen(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(NET_ERR_OK, (int)result.value.i, "listen should succeed for keepalive test");

	qd_stack_element_t socket_elem;
	qd_stack_pop(ctx->st, &socket_elem);
	int64_t sock_fd = socket_elem.value.i;

	// Enable keepalive
	qd_push_i(ctx, sock_fd);
	qd_push_i(ctx, 1);
	usr_net_set_keepalive(ctx);

	qd_stack_element_t keepalive_result;
	qd_stack_pop(ctx->st, &keepalive_result);
	ASSERT_EQ(QD_STACK_TYPE_INT, keepalive_result.type, "keepalive result should be int");
	ASSERT_EQ(NET_ERR_OK, (int)keepalive_result.value.i, "set_keepalive should succeed");

	// Clean up
	qd_push_i(ctx, sock_fd);
	usr_net_close(ctx);

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 7: Shutdown on a listening socket (no crash)
// ---------------------------------------------------------------------------

TEST(NetShutdownListeningSocketTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	usr_net_listen(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(NET_ERR_OK, (int)result.value.i, "listen should succeed for shutdown test");

	qd_stack_element_t socket_elem;
	qd_stack_pop(ctx->st, &socket_elem);
	int64_t sock_fd = socket_elem.value.i;

	// Shutdown the listening socket (should not crash)
	qd_push_i(ctx, sock_fd);
	usr_net_shutdown(ctx);

	// Close
	qd_push_i(ctx, sock_fd);
	usr_net_close(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after shutdown+close");

	destroy_test_context(ctx);
}

int main(void) {
	return UC_PrintResults();
}
