/**
 * @file test_io.c
 * @brief Unit tests for the qdio file I/O library
 *
 * Tests file operations including open, read, write, seek, and close.
 */

#include <qdio/io.h>
#include <qdrt/runtime.h>
#include <qdrt/context.h>
#include <qdrt/stack.h>
#include <qdrt/qd_string.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Helper function to create a context
static qd_context* create_test_context(void) {
	qd_context* ctx = qd_create_context(256);
	return ctx;
}

// Helper function to destroy a context
static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}

// Helper to create a temp file path
static char* create_temp_path(const char* suffix) {
	static char path[256];
	snprintf(path, sizeof(path), "/tmp/quadrate_io_test_%d_%s", getpid(), suffix);
	return path;
}


TEST(IoOpenReadTest) {
	qd_context* ctx = create_test_context();
	char* path = create_temp_path("open_read.txt");

	// First create a file to read
	FILE* fp = fopen(path, "w");
	ASSERT(fp != NULL, "should create test file");
	fputs("test content", fp);
	fclose(fp);

	// Open for reading
	qd_push_s(ctx, path);
	qd_push_s(ctx, "r");
	qd_exec_result result = usr_io_open(ctx);
	ASSERT_EQ(result.code, 0, "io_open should succeed");

	// Pop status (should be Ok=1)
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ((int)status_elem.value.i, 1, "status should be Ok");

	// Pop handle
	qd_stack_element_t handle_elem;
	qd_stack_pop(ctx->st, &handle_elem);
	ASSERT_EQ(handle_elem.type, QD_STACK_TYPE_PTR, "handle should be ptr");
	ASSERT(handle_elem.value.p != NULL, "handle should not be NULL");

	// Close
	qd_push_p(ctx, handle_elem.value.p);
	usr_io_close(ctx);

	unlink(path);
	destroy_test_context(ctx);
}

TEST(IoOpenWriteTest) {
	qd_context* ctx = create_test_context();
	char* path = create_temp_path("open_write.txt");

	// Open for writing (creates file)
	qd_push_s(ctx, path);
	qd_push_s(ctx, "w");
	qd_exec_result result = usr_io_open(ctx);
	ASSERT_EQ(result.code, 0, "io_open write should succeed");

	// Pop status and handle
	qd_stack_element_t status_elem;
	qd_stack_pop(ctx->st, &status_elem);
	ASSERT_EQ((int)status_elem.value.i, 1, "status should be Ok");

	qd_stack_element_t handle_elem;
	qd_stack_pop(ctx->st, &handle_elem);
	ASSERT(handle_elem.value.p != NULL, "handle should not be NULL");

	// Close
	qd_push_p(ctx, handle_elem.value.p);
	usr_io_close(ctx);

	unlink(path);
	destroy_test_context(ctx);
}

TEST(IoOpenNotFoundTest) {
	qd_context* ctx = create_test_context();

	// Try to open non-existent file for reading
	qd_push_s(ctx, "/nonexistent/path/file.txt");
	qd_push_s(ctx, "r");
	qd_exec_result result = usr_io_open(ctx);
	ASSERT(result.code != 0, "io_open should fail for missing file");

	// Pop error code
	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT_EQ((int)err_elem.value.i, 2, "should return NotFound (2)");

	destroy_test_context(ctx);
}

TEST(IoCloseNullTest) {
	qd_context* ctx = create_test_context();

	// Close NULL handle (should be safe no-op)
	qd_push_p(ctx, NULL);
	qd_exec_result result = usr_io_close(ctx);
	ASSERT_EQ(result.code, 0, "closing NULL should succeed");

	destroy_test_context(ctx);
}


TEST(IoWriteReadStringTest) {
	qd_context* ctx = create_test_context();
	char* path = create_temp_path("write_read.txt");
	const char* test_data = "Hello, Quadrate!";

	// Open for writing
	qd_push_s(ctx, path);
	qd_push_s(ctx, "w");
	usr_io_open(ctx);
	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	qd_stack_element_t write_handle;
	qd_stack_pop(ctx->st, &write_handle);

	// Write data
	qd_push_p(ctx, write_handle.value.p);
	qd_push_s(ctx, test_data);
	qd_exec_result result = usr_io_write_string(ctx);
	ASSERT_EQ(result.code, 0, "write should succeed");

	// Pop write status and bytes_written
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ((int)status.value.i, 1, "write status should be Ok");
	qd_stack_element_t written;
	qd_stack_pop(ctx->st, &written);
	ASSERT_EQ((int)written.value.i, (int)strlen(test_data), "should write all bytes");

	// Close write handle
	qd_push_p(ctx, write_handle.value.p);
	usr_io_close(ctx);

	// Open for reading
	qd_push_s(ctx, path);
	qd_push_s(ctx, "r");
	usr_io_open(ctx);
	qd_stack_pop(ctx->st, &status);
	qd_stack_element_t read_handle;
	qd_stack_pop(ctx->st, &read_handle);

	// Read data
	qd_push_p(ctx, read_handle.value.p);
	qd_push_i(ctx, 100);
	result = usr_io_read_string(ctx);
	ASSERT_EQ(result.code, 0, "read should succeed");

	// Pop status, bytes_read, data
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ((int)status.value.i, 1, "read status should be Ok");
	qd_stack_element_t bytes_read;
	qd_stack_pop(ctx->st, &bytes_read);
	ASSERT_EQ((int)bytes_read.value.i, (int)strlen(test_data), "should read all bytes");
	qd_stack_element_t data;
	qd_stack_pop(ctx->st, &data);
	ASSERT_EQ(data.type, QD_STACK_TYPE_STR, "data should be string");
	ASSERT_STR_EQ(qd_string_data(data.value.s), test_data, "data should match");

	qd_string_release(data.value.s);

	// Close read handle
	qd_push_p(ctx, read_handle.value.p);
	usr_io_close(ctx);

	unlink(path);
	destroy_test_context(ctx);
}

TEST(IoWriteReadEmptyTest) {
	qd_context* ctx = create_test_context();
	char* path = create_temp_path("empty.txt");

	// Create empty file
	qd_push_s(ctx, path);
	qd_push_s(ctx, "w");
	usr_io_open(ctx);
	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	qd_stack_element_t handle;
	qd_stack_pop(ctx->st, &handle);

	// Write empty string
	qd_push_p(ctx, handle.value.p);
	qd_push_s(ctx, "");
	usr_io_write_string(ctx);
	qd_stack_pop(ctx->st, &status);
	qd_stack_element_t written;
	qd_stack_pop(ctx->st, &written);
	ASSERT_EQ((int)written.value.i, 0, "should write 0 bytes");

	qd_push_p(ctx, handle.value.p);
	usr_io_close(ctx);

	// Read from empty file
	qd_push_s(ctx, path);
	qd_push_s(ctx, "r");
	usr_io_open(ctx);
	qd_stack_pop(ctx->st, &status);
	qd_stack_pop(ctx->st, &handle);

	qd_push_p(ctx, handle.value.p);
	qd_push_i(ctx, 100);
	usr_io_read_string(ctx);
	qd_stack_pop(ctx->st, &status);
	qd_stack_element_t bytes_read;
	qd_stack_pop(ctx->st, &bytes_read);
	ASSERT_EQ((int)bytes_read.value.i, 0, "should read 0 bytes from empty");
	qd_stack_element_t data;
	qd_stack_pop(ctx->st, &data);
	ASSERT_STR_EQ(qd_string_data(data.value.s), "", "should read empty string");
	qd_string_release(data.value.s);

	qd_push_p(ctx, handle.value.p);
	usr_io_close(ctx);

	unlink(path);
	destroy_test_context(ctx);
}


TEST(IoSeekTellTest) {
	qd_context* ctx = create_test_context();
	char* path = create_temp_path("seek.txt");

	// Create file with content
	qd_push_s(ctx, path);
	qd_push_s(ctx, "w");
	usr_io_open(ctx);
	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	qd_stack_element_t handle;
	qd_stack_pop(ctx->st, &handle);

	qd_push_p(ctx, handle.value.p);
	qd_push_s(ctx, "0123456789");
	usr_io_write_string(ctx);
	qd_stack_pop(ctx->st, &status);
	qd_stack_element_t written;
	qd_stack_pop(ctx->st, &written);

	qd_push_p(ctx, handle.value.p);
	usr_io_close(ctx);

	// Open for reading
	qd_push_s(ctx, path);
	qd_push_s(ctx, "r");
	usr_io_open(ctx);
	qd_stack_pop(ctx->st, &status);
	qd_stack_pop(ctx->st, &handle);

	// Seek to position 5 (SEEK_SET = 0)
	qd_push_p(ctx, handle.value.p);
	qd_push_i(ctx, 5);
	qd_push_i(ctx, 0);
	qd_exec_result result = usr_io_seek(ctx);
	ASSERT_EQ(result.code, 0, "seek should succeed");

	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ((int)status.value.i, 1, "seek status should be Ok");
	qd_stack_element_t pos;
	qd_stack_pop(ctx->st, &pos);
	ASSERT_EQ((int)pos.value.i, 5, "position should be 5");

	// Read from position 5
	qd_push_p(ctx, handle.value.p);
	qd_push_i(ctx, 5);
	usr_io_read_string(ctx);
	qd_stack_pop(ctx->st, &status);
	qd_stack_element_t bytes_read;
	qd_stack_pop(ctx->st, &bytes_read);
	qd_stack_element_t data;
	qd_stack_pop(ctx->st, &data);
	ASSERT_STR_EQ(qd_string_data(data.value.s), "56789", "should read from position 5");
	qd_string_release(data.value.s);

	qd_push_p(ctx, handle.value.p);
	usr_io_close(ctx);

	unlink(path);
	destroy_test_context(ctx);
}

TEST(IoSeekEndTest) {
	qd_context* ctx = create_test_context();
	char* path = create_temp_path("seek_end.txt");

	// Create file with 10 bytes
	FILE* fp = fopen(path, "w");
	fputs("0123456789", fp);
	fclose(fp);

	// Open for reading
	qd_push_s(ctx, path);
	qd_push_s(ctx, "r");
	usr_io_open(ctx);
	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	qd_stack_element_t handle;
	qd_stack_pop(ctx->st, &handle);

	// Seek to end (SEEK_END = 2)
	qd_push_p(ctx, handle.value.p);
	qd_push_i(ctx, 0);
	qd_push_i(ctx, 2);
	usr_io_seek(ctx);
	qd_stack_pop(ctx->st, &status);
	qd_stack_element_t pos;
	qd_stack_pop(ctx->st, &pos);
	ASSERT_EQ((int)pos.value.i, 10, "end position should be 10");

	// Seek -3 from current (SEEK_CUR = 1)
	qd_push_p(ctx, handle.value.p);
	qd_push_i(ctx, -3);
	qd_push_i(ctx, 1);
	usr_io_seek(ctx);
	qd_stack_pop(ctx->st, &status);
	qd_stack_pop(ctx->st, &pos);
	ASSERT_EQ((int)pos.value.i, 7, "position should be 7");

	qd_push_p(ctx, handle.value.p);
	usr_io_close(ctx);

	unlink(path);
	destroy_test_context(ctx);
}


TEST(IoEofTest) {
	qd_context* ctx = create_test_context();
	char* path = create_temp_path("eof.txt");

	// Create file with 5 bytes
	FILE* fp = fopen(path, "w");
	fputs("hello", fp);
	fclose(fp);

	// Open for reading
	qd_push_s(ctx, path);
	qd_push_s(ctx, "r");
	usr_io_open(ctx);
	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	qd_stack_element_t handle;
	qd_stack_pop(ctx->st, &handle);

	// Check EOF before reading (should be 0)
	qd_push_p(ctx, handle.value.p);
	usr_io_eof(ctx);
	qd_stack_element_t eof;
	qd_stack_pop(ctx->st, &eof);
	ASSERT_EQ((int)eof.value.i, 0, "EOF should be 0 at start");

	// Pop handle that was left on stack by eof
	qd_stack_pop(ctx->st, &handle);

	// Read all content
	qd_push_p(ctx, handle.value.p);
	qd_push_i(ctx, 10);
	usr_io_read_string(ctx);
	qd_stack_pop(ctx->st, &status);
	qd_stack_element_t bytes_read;
	qd_stack_pop(ctx->st, &bytes_read);
	qd_stack_element_t data;
	qd_stack_pop(ctx->st, &data);
	qd_string_release(data.value.s);

	// Check EOF after reading to end (should be 1)
	qd_push_p(ctx, handle.value.p);
	usr_io_eof(ctx);
	qd_stack_pop(ctx->st, &eof);
	ASSERT_EQ((int)eof.value.i, 1, "EOF should be 1 after reading all");

	// Pop handle left on stack
	qd_stack_pop(ctx->st, &handle);

	qd_push_p(ctx, handle.value.p);
	usr_io_close(ctx);

	unlink(path);
	destroy_test_context(ctx);
}


TEST(IoReadFileTest) {
	qd_context* ctx = create_test_context();
	char* path = create_temp_path("read_file.txt");
	const char* content = "Full file content\nwith multiple lines\n";

	// Create file with content
	FILE* fp = fopen(path, "w");
	fputs(content, fp);
	fclose(fp);

	// Read entire file
	qd_push_s(ctx, path);
	qd_exec_result result = usr_io_read_file(ctx);
	ASSERT_EQ(result.code, 0, "read_file should succeed");

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ((int)status.value.i, 1, "status should be Ok");

	qd_stack_element_t data;
	qd_stack_pop(ctx->st, &data);
	ASSERT_EQ(data.type, QD_STACK_TYPE_STR, "data should be string");
	ASSERT_STR_EQ(qd_string_data(data.value.s), content, "content should match");
	qd_string_release(data.value.s);

	unlink(path);
	destroy_test_context(ctx);
}

TEST(IoWriteFileTest) {
	qd_context* ctx = create_test_context();
	char* path = create_temp_path("write_file.txt");
	const char* content = "Written by write_file";

	// Write file
	qd_push_s(ctx, path);
	qd_push_s(ctx, content);
	qd_exec_result result = usr_io_write_file(ctx);
	ASSERT_EQ(result.code, 0, "write_file should succeed");

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ((int)status.value.i, 1, "status should be Ok");

	// Verify by reading back
	FILE* fp = fopen(path, "r");
	ASSERT(fp != NULL, "file should exist");
	char buffer[256];
	char* line = fgets(buffer, sizeof(buffer), fp);
	ASSERT(line != NULL, "should read content");
	ASSERT_STR_EQ(buffer, content, "content should match");
	fclose(fp);

	unlink(path);
	destroy_test_context(ctx);
}

TEST(IoReadFileNotFoundTest) {
	qd_context* ctx = create_test_context();

	// Try to read non-existent file
	qd_push_s(ctx, "/nonexistent/path/file.txt");
	qd_exec_result result = usr_io_read_file(ctx);
	ASSERT(result.code != 0, "read_file should fail for missing file");

	qd_stack_element_t err;
	qd_stack_pop(ctx->st, &err);
	ASSERT_EQ((int)err.value.i, 2, "should return NotFound (2)");

	destroy_test_context(ctx);
}


TEST(IoMemoryTest) {
	qd_context* ctx = create_test_context();
	char* path = create_temp_path("memory.txt");

	// Create, write, read, close multiple times
	for (int i = 0; i < 10; i++) {
		// Write
		qd_push_s(ctx, path);
		qd_push_s(ctx, "w");
		usr_io_open(ctx);
		qd_stack_element_t status;
		qd_stack_pop(ctx->st, &status);
		qd_stack_element_t handle;
		qd_stack_pop(ctx->st, &handle);

		qd_push_p(ctx, handle.value.p);
		qd_push_s(ctx, "test data for memory check");
		usr_io_write_string(ctx);
		qd_stack_pop(ctx->st, &status);
		qd_stack_element_t written;
		qd_stack_pop(ctx->st, &written);

		qd_push_p(ctx, handle.value.p);
		usr_io_close(ctx);

		// Read
		qd_push_s(ctx, path);
		qd_push_s(ctx, "r");
		usr_io_open(ctx);
		qd_stack_pop(ctx->st, &status);
		qd_stack_pop(ctx->st, &handle);

		qd_push_p(ctx, handle.value.p);
		qd_push_i(ctx, 100);
		usr_io_read_string(ctx);
		qd_stack_pop(ctx->st, &status);
		qd_stack_element_t bytes_read;
		qd_stack_pop(ctx->st, &bytes_read);
		qd_stack_element_t data;
		qd_stack_pop(ctx->st, &data);
		qd_string_release(data.value.s);

		qd_push_p(ctx, handle.value.p);
		usr_io_close(ctx);
	}

	unlink(path);
	destroy_test_context(ctx);
}

// Main - required for test executable
int main(void) {
	return UC_PrintResults();
}
