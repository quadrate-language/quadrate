/**
 * @file test_net.c
 * @brief Unit tests for the qdnet network library
 *
 * Tests basic socket operations using localhost connections.
 */

#define _DEFAULT_SOURCE  // For usleep
#include <qdnet/net.h>
#include <qdrt/runtime.h>
#include <qdrt/context.h>
#include <qdrt/stack.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// Helper function to create a context
static qd_context* create_test_context(void) {
	qd_context* ctx = (qd_context*)malloc(sizeof(qd_context));
	qd_stack_init(&ctx->st, 256);
	return ctx;
}

// Helper function to destroy a context
static void destroy_test_context(qd_context* ctx) {
	qd_stack_destroy(ctx->st);
	free(ctx);
}

// Test port - use high port to avoid permission issues
static int test_port = 18080;

// ========== Listen Tests ==========

TEST(ListenBasicTest) {
	qd_context* ctx = create_test_context();

	// Push port
	qd_push_i(ctx, test_port);

	// Call listen
	qd_exec_result result = usr_net_listen(ctx);
	ASSERT_EQ(result.code, 0, "listen should succeed");

	// Pop Ok status first (success pushes [result, Ok])
	qd_stack_element_t status_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop status should succeed");
	ASSERT_EQ((int)status_elem.value.i, 1, "status should be Ok (1)");

	// Check that a valid socket was returned
	qd_stack_element_t socket_elem;
	err = qd_stack_pop(ctx->st, &socket_elem);
	ASSERT_EQ(err, QD_STACK_OK, "pop should succeed");
	ASSERT_EQ(socket_elem.type, QD_STACK_TYPE_INT, "socket should be int");
	ASSERT(socket_elem.value.i >= 0, "socket fd should be non-negative");

	// Close the socket
	qd_push_i(ctx, socket_elem.value.i);
	usr_net_close(ctx);

	// Increment port for next test
	test_port++;

	destroy_test_context(ctx);
}

// ========== Close Tests ==========

TEST(CloseBasicTest) {
	qd_context* ctx = create_test_context();

	// Create a listening socket first
	qd_push_i(ctx, test_port);
	usr_net_listen(ctx);

	// Pop Ok status first (success pushes [result, Ok])
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);

	// Get the socket fd
	qd_stack_element_t socket_elem;
	qd_stack_pop(ctx->st, &socket_elem);
	int64_t socket_fd = socket_elem.value.i;

	// Close it
	qd_push_i(ctx, socket_fd);
	qd_exec_result result = usr_net_close(ctx);
	ASSERT_EQ(result.code, 0, "close should succeed");

	// Increment port for next test
	test_port++;

	destroy_test_context(ctx);
}

TEST(ShutdownBasicTest) {
	qd_context* ctx = create_test_context();

	// Create a listening socket first
	qd_push_i(ctx, test_port);
	usr_net_listen(ctx);

	// Pop Ok status first (success pushes [result, Ok])
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);

	// Get the socket fd
	qd_stack_element_t socket_elem;
	qd_stack_pop(ctx->st, &socket_elem);
	int64_t socket_fd = socket_elem.value.i;

	// Shutdown it
	qd_push_i(ctx, socket_fd);
	qd_exec_result result = usr_net_shutdown(ctx);
	ASSERT_EQ(result.code, 0, "shutdown should succeed");

	// Close it
	qd_push_i(ctx, socket_fd);
	usr_net_close(ctx);

	// Increment port for next test
	test_port++;

	destroy_test_context(ctx);
}

// ========== Client-Server Integration Test ==========

// Server thread function
static void* server_thread(void* arg) {
	int port = *(int*)arg;

	qd_context* ctx = create_test_context();

	// Listen on port
	qd_push_i(ctx, port);
	usr_net_listen(ctx);

	// Pop Ok status, then socket
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);

	qd_stack_element_t server_elem;
	qd_stack_pop(ctx->st, &server_elem);
	int64_t server_fd = server_elem.value.i;

	// Accept connection
	qd_push_i(ctx, server_fd);
	usr_net_accept(ctx);

	// Pop Ok status, then client socket
	qd_stack_pop(ctx->st, &status_elem);

	qd_stack_element_t client_elem;
	qd_stack_pop(ctx->st, &client_elem);
	int64_t client_fd = client_elem.value.i;

	// Receive data
	qd_push_i(ctx, client_fd);
	qd_push_i(ctx, 1024);
	usr_net_receive(ctx);

	// Pop Ok status, then bytes read and data
	qd_stack_pop(ctx->st, &status_elem);

	qd_stack_element_t bytes_elem;
	qd_stack_pop(ctx->st, &bytes_elem);

	qd_stack_element_t data_elem;
	qd_stack_pop(ctx->st, &data_elem);

	// Send response
	qd_push_i(ctx, client_fd);
	qd_push_s(ctx, "PONG");
	usr_net_send(ctx);

	// Pop Ok status, then bytes sent
	qd_stack_pop(ctx->st, &status_elem);

	qd_stack_element_t sent_elem;
	qd_stack_pop(ctx->st, &sent_elem);

	// Release received string
	if (data_elem.type == QD_STACK_TYPE_STR) {
		qd_string_release(data_elem.value.s);
	}

	// Clean up
	qd_push_i(ctx, client_fd);
	usr_net_close(ctx);

	qd_push_i(ctx, server_fd);
	usr_net_close(ctx);

	destroy_test_context(ctx);
	return NULL;
}

TEST(ClientServerIntegrationTest) {
	int port = test_port++;
	pthread_t server_tid;

	// Start server thread
	pthread_create(&server_tid, NULL, server_thread, &port);

	// Give server time to start
	usleep(100000);  // 100ms

	qd_context* ctx = create_test_context();

	// Connect to server
	qd_push_s(ctx, "127.0.0.1");
	qd_push_i(ctx, port);
	qd_exec_result result = usr_net_connect(ctx);
	ASSERT_EQ(result.code, 0, "connect should succeed");

	// Pop Ok status first
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ((int)status_elem.value.i, 1, "status should be Ok (1)");

	qd_stack_element_t socket_elem;
	qd_stack_pop(ctx->st, &socket_elem);
	int64_t socket_fd = socket_elem.value.i;
	ASSERT(socket_fd >= 0, "socket fd should be valid");

	// Send data
	qd_push_i(ctx, socket_fd);
	qd_push_s(ctx, "PING");
	result = usr_net_send(ctx);
	ASSERT_EQ(result.code, 0, "send should succeed");

	// Pop Ok status first
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ((int)status_elem.value.i, 1, "status should be Ok (1)");

	qd_stack_element_t bytes_sent_elem;
	qd_stack_pop(ctx->st, &bytes_sent_elem);
	ASSERT_EQ((int)bytes_sent_elem.value.i, 4, "should send 4 bytes");

	// Receive response
	qd_push_i(ctx, socket_fd);
	qd_push_i(ctx, 1024);
	result = usr_net_receive(ctx);
	ASSERT_EQ(result.code, 0, "receive should succeed");

	// Pop Ok status first
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ((int)status_elem.value.i, 1, "status should be Ok (1)");

	// Get bytes read
	qd_stack_element_t bytes_read_elem;
	qd_stack_pop(ctx->st, &bytes_read_elem);
	ASSERT_EQ((int)bytes_read_elem.value.i, 4, "should receive 4 bytes");

	// Get data
	qd_stack_element_t data_elem;
	qd_stack_pop(ctx->st, &data_elem);
	ASSERT_EQ(data_elem.type, QD_STACK_TYPE_STR, "data should be string");
	ASSERT_STR_EQ(qd_string_data(data_elem.value.s), "PONG", "should receive PONG");
	qd_string_release(data_elem.value.s);

	// Close socket
	qd_push_i(ctx, socket_fd);
	usr_net_close(ctx);

	// Wait for server thread
	pthread_join(server_tid, NULL);

	destroy_test_context(ctx);
}

// Main - required for test executable
int main(void) {
	return UC_PrintResults();
}
