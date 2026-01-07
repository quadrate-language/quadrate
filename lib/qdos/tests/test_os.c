/**
 * @file test_os.c
 * @brief Unit tests for the qdos operating system library
 */

#define _POSIX_C_SOURCE 200809L

#include <qdos/os.h>
#include <qdrt/runtime.h>
#include <qdrt/context.h>
#include <qdrt/stack.h>
#include <qdrt/qd_string.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static qd_context* create_test_context(void) {
	return qd_create_context(256);
}

static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}


TEST(OsGetenvPathTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "PATH");
	usr_os_getenv(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "result should be string");
	// PATH should contain at least one character
	ASSERT_TRUE(strlen(qd_string_data(elem.value.s)) > 0, "PATH should not be empty");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(OsGetenvNotSetTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "QUADRATE_NONEXISTENT_VAR_XYZ123");
	usr_os_getenv(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "result should be string");
	ASSERT_STR_EQ("", qd_string_data(elem.value.s), "unset var should return empty");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


TEST(OsSetenvGetenvTest) {
	qd_context* ctx = create_test_context();

	// Set a variable
	qd_push_s(ctx, "QUADRATE_TEST_VAR");
	qd_push_s(ctx, "test_value_123");
	usr_os_setenv(ctx);

	// Get it back
	qd_push_s(ctx, "QUADRATE_TEST_VAR");
	usr_os_getenv(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("test_value_123", qd_string_data(elem.value.s), "setenv/getenv roundtrip");

	qd_string_release(elem.value.s);

	// Clean up
	unsetenv("QUADRATE_TEST_VAR");
	destroy_test_context(ctx);
}


TEST(OsExistsRootTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "/");
	usr_os_exists(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "root should exist");

	destroy_test_context(ctx);
}

TEST(OsExistsTmpTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "/tmp");
	usr_os_exists(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "/tmp should exist");

	destroy_test_context(ctx);
}

TEST(OsExistsNotFoundTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "/nonexistent_path_xyz_123");
	usr_os_exists(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "nonexistent path should return 0");

	destroy_test_context(ctx);
}


TEST(OsIsDirTmpTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "/tmp");
	usr_os_is_dir(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "/tmp should be a directory");

	destroy_test_context(ctx);
}

TEST(OsIsDirFileTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "/etc/passwd");
	usr_os_is_dir(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "/etc/passwd should not be a directory");

	destroy_test_context(ctx);
}


TEST(OsIsFilePasswdTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "/etc/passwd");
	usr_os_is_file(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "/etc/passwd should be a file");

	destroy_test_context(ctx);
}

TEST(OsIsFileDirTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "/tmp");
	usr_os_is_file(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "/tmp should not be a file");

	destroy_test_context(ctx);
}


TEST(OsGetpidTest) {
	qd_context* ctx = create_test_context();

	usr_os_getpid(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "result should be int");
	ASSERT_TRUE(elem.value.i > 0, "pid should be positive");
	ASSERT_EQ(getpid(), (int)elem.value.i, "pid should match getpid()");

	destroy_test_context(ctx);
}


TEST(OsCwdTest) {
	qd_context* ctx = create_test_context();

	usr_os_cwd(ctx);

	// Pop status first
	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(OS_ERR_OK, (int)status.value.i, "cwd should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "result should be string");
	// CWD should start with /
	ASSERT_TRUE(qd_string_data(elem.value.s)[0] == '/', "cwd should start with /");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


TEST(OsMktempTest) {
	qd_context* ctx = create_test_context();

	usr_os_mktemp(ctx);

	// Pop status first
	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(OS_ERR_OK, (int)status.value.i, "mktemp should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "result should be string");

	const char* tmpdir = qd_string_data(elem.value.s);
	// Should start with /tmp
	ASSERT_TRUE(strncmp(tmpdir, "/tmp", 4) == 0, "mktemp should create in /tmp");

	// Directory should exist
	qd_push_s(ctx, tmpdir);
	usr_os_exists(ctx);
	qd_stack_element_t exists_elem;
	qd_stack_pop(ctx->st, &exists_elem);
	ASSERT_EQ(1, (int)exists_elem.value.i, "temp dir should exist");

	// Clean up - remove the temp directory
	qd_push_s(ctx, tmpdir);
	usr_os_rmdir(ctx);

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


TEST(OsMkdirRmdirTest) {
	qd_context* ctx = create_test_context();

	const char* testdir = "/tmp/quadrate_test_dir_xyz";

	// Create directory
	qd_push_s(ctx, testdir);
	usr_os_mkdir(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(OS_ERR_OK, (int)result.value.i, "mkdir should succeed");

	// Verify it exists
	qd_push_s(ctx, testdir);
	usr_os_is_dir(ctx);
	qd_stack_element_t is_dir_elem;
	qd_stack_pop(ctx->st, &is_dir_elem);
	ASSERT_EQ(1, (int)is_dir_elem.value.i, "created dir should be a directory");

	// Remove it
	qd_push_s(ctx, testdir);
	usr_os_rmdir(ctx);

	// Verify it's gone
	qd_push_s(ctx, testdir);
	usr_os_exists(ctx);
	qd_stack_element_t exists_elem;
	qd_stack_pop(ctx->st, &exists_elem);
	ASSERT_EQ(0, (int)exists_elem.value.i, "removed dir should not exist");

	destroy_test_context(ctx);
}

int main(void) {
	return UC_PrintResults();
}
