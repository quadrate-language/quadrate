/**
 * @file test_tls.c
 * @brief Unit tests for the qdtls TLS library
 *
 * Tests TLS operations including connection, send/receive, and error handling.
 * Some tests require network access.
 */

#include <qdtls/tls.h>
#include <qdrt/runtime.h>
#include <qdrt/context.h>
#include <qdrt/stack.h>
#include <qdrt/qd_string.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

// Helper function to create a context
static qd_context* create_test_context(void) {
	qd_context* ctx = qd_create_context(256);
	return ctx;
}

// Helper function to destroy a context
static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}

// Helper to create TCP connection to host:port
static int create_tcp_socket(const char* host, int port) {
	struct hostent* he = gethostbyname(host);
	if (!he) return -1;

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) return -1;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)port);
	memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		close(sock);
		return -1;
	}

	return sock;
}

// ========== Close Tests ==========

TEST(TlsCloseNullTest) {
	qd_context* ctx = create_test_context();

	// Close NULL connection (should be safe no-op)
	qd_push_p(ctx, NULL);
	qd_exec_result result = usr_tls_close(ctx);
	ASSERT_EQ(result.code, 0, "closing NULL should succeed");

	destroy_test_context(ctx);
}

// ========== Connect Tests (require network) ==========

TEST(TlsConnectBasicTest) {
	qd_context* ctx = create_test_context();

	// Create TCP socket to example.com:443
	int sock = create_tcp_socket("example.com", 443);
	ASSERT(sock >= 0, "TCP connection should succeed");

	// Connect TLS
	qd_push_i(ctx, sock);
	qd_push_s(ctx, "example.com");
	qd_exec_result result = usr_tls_connect(ctx);
	ASSERT_EQ(result.code, 0, "TLS connect should succeed");

	// Pop status (should be Ok=1)
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ(status_elem.type, QD_STACK_TYPE_INT, "status should be int");
	ASSERT_EQ((int)status_elem.value.i, 1, "status should be Ok (1)");

	// Pop connection pointer
	qd_stack_element_t conn_elem;
	qd_stack_pop(ctx->st, &conn_elem);
	ASSERT_EQ(conn_elem.type, QD_STACK_TYPE_PTR, "connection should be ptr");
	ASSERT(conn_elem.value.p != NULL, "connection should not be NULL");

	// Close TLS connection
	qd_push_p(ctx, conn_elem.value.p);
	usr_tls_close(ctx);

	// Close TCP socket
	close(sock);

	destroy_test_context(ctx);
}

TEST(TlsSendReceiveTest) {
	qd_context* ctx = create_test_context();

	// Create TCP socket to example.com:443
	int sock = create_tcp_socket("example.com", 443);
	ASSERT(sock >= 0, "TCP connection should succeed");

	// Connect TLS
	qd_push_i(ctx, sock);
	qd_push_s(ctx, "example.com");
	usr_tls_connect(ctx);

	// Pop status and connection
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	qd_stack_element_t conn_elem;
	qd_stack_pop(ctx->st, &conn_elem);

	// Send HTTP request
	qd_push_p(ctx, conn_elem.value.p);
	qd_push_s(ctx, "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n");
	qd_exec_result result = usr_tls_send(ctx);
	ASSERT_EQ(result.code, 0, "TLS send should succeed");

	// Pop send status and bytes_sent
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ((int)status_elem.value.i, 1, "send status should be Ok");
	qd_stack_element_t sent_elem;
	qd_stack_pop(ctx->st, &sent_elem);
	ASSERT(sent_elem.value.i > 0, "bytes_sent should be positive");

	// Receive response
	qd_push_p(ctx, conn_elem.value.p);
	qd_push_i(ctx, 4096);
	result = usr_tls_receive(ctx);
	ASSERT_EQ(result.code, 0, "TLS receive should succeed");

	// Pop receive status, bytes_read, data
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ((int)status_elem.value.i, 1, "receive status should be Ok");
	qd_stack_element_t read_elem;
	qd_stack_pop(ctx->st, &read_elem);
	ASSERT(read_elem.value.i > 0, "bytes_read should be positive");
	qd_stack_element_t data_elem;
	qd_stack_pop(ctx->st, &data_elem);
	ASSERT_EQ(data_elem.type, QD_STACK_TYPE_STR, "data should be string");

	// Verify response contains HTTP
	const char* data = qd_string_data(data_elem.value.s);
	ASSERT(strstr(data, "HTTP/1.1") != NULL, "response should contain HTTP/1.1");

	qd_string_release(data_elem.value.s);

	// Close TLS connection
	qd_push_p(ctx, conn_elem.value.p);
	usr_tls_close(ctx);

	close(sock);
	destroy_test_context(ctx);
}

TEST(TlsConnectBadHostTest) {
	qd_context* ctx = create_test_context();

	// Create TCP socket to example.com:443
	int sock = create_tcp_socket("example.com", 443);
	ASSERT(sock >= 0, "TCP connection should succeed");

	// Try to connect with wrong hostname (should fail cert verification)
	qd_push_i(ctx, sock);
	qd_push_s(ctx, "wrong-hostname.invalid");
	usr_tls_connect(ctx);

	// Should fail - pop error code from stack
	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT(err_elem.value.i > 1, "should return error code > 1");

	close(sock);
	destroy_test_context(ctx);
}

// ========== Memory Management Tests ==========

TEST(TlsConnectionMemoryTest) {
	qd_context* ctx = create_test_context();

	// Create multiple connections and close them
	for (int i = 0; i < 3; i++) {
		int sock = create_tcp_socket("example.com", 443);
		if (sock < 0) continue;  // Skip if network unavailable

		qd_push_i(ctx, sock);
		qd_push_s(ctx, "example.com");
		qd_exec_result conn_result = usr_tls_connect(ctx);

		if (conn_result.code == 0) {
			// Pop status and connection
			qd_stack_element_t status_elem;
			qd_stack_pop(ctx->st, &status_elem);
			qd_stack_element_t conn_elem;
			qd_stack_pop(ctx->st, &conn_elem);

			// Close TLS
			qd_push_p(ctx, conn_elem.value.p);
			usr_tls_close(ctx);
		} else {
			// Pop error code
			qd_stack_element_t err_elem;
			qd_stack_pop(ctx->st, &err_elem);
		}

		close(sock);
	}

	destroy_test_context(ctx);
}

// Main - required for test executable
int main(void) {
	return UC_PrintResults();
}
