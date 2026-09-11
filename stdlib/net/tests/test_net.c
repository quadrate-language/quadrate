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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

// ---------------------------------------------------------------------------
// Test 8: DNS lookup of "127.0.0.1" (IP literal should resolve to itself)
// ---------------------------------------------------------------------------

TEST(NetLookupIPLiteralTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "127.0.0.1");
	usr_net_lookup(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(QD_STACK_TYPE_INT, result.type, "lookup result should be int");
	ASSERT_EQ(NET_ERR_OK, (int)result.value.i, "lookup of 127.0.0.1 should succeed");

	qd_stack_element_t ip;
	qd_stack_pop(ctx->st, &ip);
	ASSERT_EQ(QD_STACK_TYPE_STR, ip.type, "lookup ip should be string");
	ASSERT_STR_EQ("127.0.0.1", qd_string_data(ip.value.s), "127.0.0.1 should resolve to itself");
	qd_string_release(ip.value.s);

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 9: DNS lookup of empty string should fail
// ---------------------------------------------------------------------------

TEST(NetLookupEmptyStringTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	usr_net_lookup(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(QD_STACK_TYPE_INT, result.type, "lookup result should be int");
	// Empty string lookup should fail (not NET_ERR_OK)
	ASSERT_NE(NET_ERR_OK, (int)result.value.i, "lookup of empty string should fail");

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 10: Listen on port 0 twice - should get different sockets
// ---------------------------------------------------------------------------

TEST(NetListenTwiceDifferentSocketsTest) {
	qd_context* ctx = create_test_context();

	// First listen
	qd_push_i(ctx, 0);
	usr_net_listen(ctx);
	qd_stack_element_t result1;
	qd_stack_pop(ctx->st, &result1);
	ASSERT_EQ(NET_ERR_OK, (int)result1.value.i, "first listen should succeed");
	qd_stack_element_t sock1;
	qd_stack_pop(ctx->st, &sock1);
	ASSERT_TRUE(sock1.value.i > 0, "first socket fd should be positive");

	// Second listen
	qd_push_i(ctx, 0);
	usr_net_listen(ctx);
	qd_stack_element_t result2;
	qd_stack_pop(ctx->st, &result2);
	ASSERT_EQ(NET_ERR_OK, (int)result2.value.i, "second listen should succeed");
	qd_stack_element_t sock2;
	qd_stack_pop(ctx->st, &sock2);
	ASSERT_TRUE(sock2.value.i > 0, "second socket fd should be positive");

	// The two socket fds should be different
	ASSERT_NE((int)sock1.value.i, (int)sock2.value.i, "two listens should produce different sockets");

	// Clean up both
	qd_push_i(ctx, sock1.value.i);
	usr_net_close(ctx);
	qd_push_i(ctx, sock2.value.i);
	usr_net_close(ctx);

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 11: Set timeout with 0ms
// ---------------------------------------------------------------------------

TEST(NetSetTimeoutZeroTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	usr_net_listen(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(NET_ERR_OK, (int)result.value.i, "listen should succeed for zero timeout test");

	qd_stack_element_t socket_elem;
	qd_stack_pop(ctx->st, &socket_elem);
	int64_t sock_fd = socket_elem.value.i;

	// Set a 0ms timeout
	qd_push_i(ctx, sock_fd);
	qd_push_i(ctx, 0);
	usr_net_set_timeout(ctx);

	qd_stack_element_t timeout_result;
	qd_stack_pop(ctx->st, &timeout_result);
	ASSERT_EQ(QD_STACK_TYPE_INT, timeout_result.type, "timeout result should be int");
	// 0ms timeout should still succeed (disables timeout)
	ASSERT_EQ(NET_ERR_OK, (int)timeout_result.value.i, "set_timeout with 0 should succeed");

	qd_push_i(ctx, sock_fd);
	usr_net_close(ctx);

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 12: Set timeout with large value (60 seconds)
// ---------------------------------------------------------------------------

TEST(NetSetTimeoutLargeTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	usr_net_listen(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(NET_ERR_OK, (int)result.value.i, "listen should succeed for large timeout test");

	qd_stack_element_t socket_elem;
	qd_stack_pop(ctx->st, &socket_elem);
	int64_t sock_fd = socket_elem.value.i;

	// Set a 60000ms (60 second) timeout
	qd_push_i(ctx, sock_fd);
	qd_push_i(ctx, 60000);
	usr_net_set_timeout(ctx);

	qd_stack_element_t timeout_result;
	qd_stack_pop(ctx->st, &timeout_result);
	ASSERT_EQ(QD_STACK_TYPE_INT, timeout_result.type, "timeout result should be int");
	ASSERT_EQ(NET_ERR_OK, (int)timeout_result.value.i, "set_timeout with 60s should succeed");

	qd_push_i(ctx, sock_fd);
	usr_net_close(ctx);

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 13: Disable keepalive (set to 0)
// ---------------------------------------------------------------------------

TEST(NetDisableKeepaliveTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	usr_net_listen(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(NET_ERR_OK, (int)result.value.i, "listen should succeed for disable keepalive test");

	qd_stack_element_t socket_elem;
	qd_stack_pop(ctx->st, &socket_elem);
	int64_t sock_fd = socket_elem.value.i;

	// Disable keepalive
	qd_push_i(ctx, sock_fd);
	qd_push_i(ctx, 0);
	usr_net_set_keepalive(ctx);

	qd_stack_element_t keepalive_result;
	qd_stack_pop(ctx->st, &keepalive_result);
	ASSERT_EQ(QD_STACK_TYPE_INT, keepalive_result.type, "keepalive result should be int");
	ASSERT_EQ(NET_ERR_OK, (int)keepalive_result.value.i, "disable keepalive should succeed");

	qd_push_i(ctx, sock_fd);
	usr_net_close(ctx);

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 14: Lookup of an invalid hostname should fail
// ---------------------------------------------------------------------------

TEST(NetLookupInvalidHostTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "this.host.does.not.exist.invalid");
	usr_net_lookup(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(QD_STACK_TYPE_INT, result.type, "lookup result should be int");
	ASSERT_NE(NET_ERR_OK, (int)result.value.i, "lookup of invalid host should fail");

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 15: Loopback test - listen, connect, accept, send, receive
// ---------------------------------------------------------------------------

TEST(NetLoopbackTest) {
	qd_context* ctx = create_test_context();

	// Listen on port 0
	qd_push_i(ctx, 0);
	usr_net_listen(ctx);

	qd_stack_element_t listen_result;
	qd_stack_pop(ctx->st, &listen_result);
	ASSERT_EQ(NET_ERR_OK, (int)listen_result.value.i, "listen should succeed for loopback");

	qd_stack_element_t server_sock;
	qd_stack_pop(ctx->st, &server_sock);
	int64_t server_fd = server_sock.value.i;
	ASSERT_TRUE(server_fd > 0, "server fd should be positive");

	// Get the assigned port using getsockname
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	int getsock_res = getsockname((int)server_fd, (struct sockaddr*)&addr, &addr_len);
	ASSERT_EQ(0, getsock_res, "getsockname should succeed");
	int port = ntohs(addr.sin_port);
	ASSERT_TRUE(port > 0, "assigned port should be positive");

	// Set a short timeout on the server socket so accept won't block forever
	qd_push_i(ctx, server_fd);
	qd_push_i(ctx, 2000);
	usr_net_set_timeout(ctx);
	qd_stack_element_t timeout_res;
	qd_stack_pop(ctx->st, &timeout_res);
	ASSERT_EQ(NET_ERR_OK, (int)timeout_res.value.i, "set_timeout on server should succeed");

	// Connect to the listening socket
	qd_push_s(ctx, "127.0.0.1");
	qd_push_i(ctx, port);
	usr_net_connect(ctx);

	qd_stack_element_t connect_result;
	qd_stack_pop(ctx->st, &connect_result);
	ASSERT_EQ(NET_ERR_OK, (int)connect_result.value.i, "connect to loopback should succeed");

	qd_stack_element_t client_sock;
	qd_stack_pop(ctx->st, &client_sock);
	int64_t client_fd = client_sock.value.i;
	ASSERT_TRUE(client_fd > 0, "client fd should be positive");

	// Accept the connection
	qd_push_i(ctx, server_fd);
	usr_net_accept(ctx);

	qd_stack_element_t accept_result;
	qd_stack_pop(ctx->st, &accept_result);
	ASSERT_EQ(NET_ERR_OK, (int)accept_result.value.i, "accept should succeed");

	qd_stack_element_t accepted_sock;
	qd_stack_pop(ctx->st, &accepted_sock);
	int64_t accepted_fd = accepted_sock.value.i;
	ASSERT_TRUE(accepted_fd > 0, "accepted fd should be positive");

	// Send data from client
	qd_push_i(ctx, client_fd);
	qd_push_s(ctx, "hello");
	usr_net_send(ctx);

	qd_stack_element_t send_result;
	qd_stack_pop(ctx->st, &send_result);
	ASSERT_EQ(NET_ERR_OK, (int)send_result.value.i, "send should succeed");

	qd_stack_element_t bytes_sent;
	qd_stack_pop(ctx->st, &bytes_sent);
	ASSERT_EQ(5, (int)bytes_sent.value.i, "should send 5 bytes");

	// Set timeout on accepted socket for receive
	qd_push_i(ctx, accepted_fd);
	qd_push_i(ctx, 2000);
	usr_net_set_timeout(ctx);
	qd_stack_element_t timeout_res2;
	qd_stack_pop(ctx->st, &timeout_res2);

	// Receive data on accepted socket
	qd_push_i(ctx, accepted_fd);
	qd_push_i(ctx, 1024);
	usr_net_receive(ctx);

	qd_stack_element_t recv_result;
	qd_stack_pop(ctx->st, &recv_result);
	ASSERT_EQ(NET_ERR_OK, (int)recv_result.value.i, "receive should succeed");

	qd_stack_element_t bytes_read;
	qd_stack_pop(ctx->st, &bytes_read);
	ASSERT_EQ(5, (int)bytes_read.value.i, "should receive 5 bytes");

	qd_stack_element_t data;
	qd_stack_pop(ctx->st, &data);
	ASSERT_EQ(QD_STACK_TYPE_STR, data.type, "received data should be string");
	ASSERT_STR_EQ("hello", qd_string_data(data.value.s), "received data should match sent");
	qd_string_release(data.value.s);

	// Clean up all sockets
	qd_push_i(ctx, client_fd);
	usr_net_close(ctx);
	qd_push_i(ctx, accepted_fd);
	usr_net_close(ctx);
	qd_push_i(ctx, server_fd);
	usr_net_close(ctx);

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 16: Enable then disable keepalive on same socket
// ---------------------------------------------------------------------------

TEST(NetKeepaliveToggleTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	usr_net_listen(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(NET_ERR_OK, (int)result.value.i, "listen should succeed for keepalive toggle");

	qd_stack_element_t socket_elem;
	qd_stack_pop(ctx->st, &socket_elem);
	int64_t sock_fd = socket_elem.value.i;

	// Enable keepalive
	qd_push_i(ctx, sock_fd);
	qd_push_i(ctx, 1);
	usr_net_set_keepalive(ctx);
	qd_stack_element_t r1;
	qd_stack_pop(ctx->st, &r1);
	ASSERT_EQ(NET_ERR_OK, (int)r1.value.i, "enable keepalive should succeed");

	// Disable keepalive
	qd_push_i(ctx, sock_fd);
	qd_push_i(ctx, 0);
	usr_net_set_keepalive(ctx);
	qd_stack_element_t r2;
	qd_stack_pop(ctx->st, &r2);
	ASSERT_EQ(NET_ERR_OK, (int)r2.value.i, "disable keepalive should succeed");

	qd_push_i(ctx, sock_fd);
	usr_net_close(ctx);

	destroy_test_context(ctx);
}

// ---------------------------------------------------------------------------
// Test 17: DNS lookup of a known resolvable name
// ---------------------------------------------------------------------------

TEST(NetLookupLoopbackIPv6Test) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "::1");
	usr_net_lookup(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(QD_STACK_TYPE_INT, result.type, "lookup result should be int");
	// ::1 should resolve successfully on most systems
	ASSERT_EQ(NET_ERR_OK, (int)result.value.i, "lookup of ::1 should succeed");

	qd_stack_element_t ip;
	qd_stack_pop(ctx->st, &ip);
	ASSERT_EQ(QD_STACK_TYPE_STR, ip.type, "lookup ip should be string");
	ASSERT_TRUE(strlen(qd_string_data(ip.value.s)) > 0, "resolved ip should not be empty");
	qd_string_release(ip.value.s);

	destroy_test_context(ctx);
}

int main(void) {
	return UC_PrintResults();
}
