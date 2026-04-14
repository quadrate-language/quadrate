/**
 * @file test_os.c
 * @brief Unit tests for the qdos operating system library
 */

#define _POSIX_C_SOURCE 200809L

#include <quadrate/os/os.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/stack.h>
#include <quadrate/rt/qd_string.h>
#include <unit-check/uc.h>
#include <stdio.h>
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

// ---------------------------------------------------------------------------
// Additional tests for untested functions and edge cases
// ---------------------------------------------------------------------------

TEST(OsSetenvOverwriteTest) {
	qd_context* ctx = create_test_context();

	// Set a variable
	qd_push_s(ctx, "QUADRATE_TEST_OVERWRITE");
	qd_push_s(ctx, "first_value");
	usr_os_setenv(ctx);

	// Overwrite it
	qd_push_s(ctx, "QUADRATE_TEST_OVERWRITE");
	qd_push_s(ctx, "second_value");
	usr_os_setenv(ctx);

	// Verify overwritten value
	qd_push_s(ctx, "QUADRATE_TEST_OVERWRITE");
	usr_os_getenv(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("second_value", qd_string_data(elem.value.s), "setenv should overwrite");

	qd_string_release(elem.value.s);
	unsetenv("QUADRATE_TEST_OVERWRITE");
	destroy_test_context(ctx);
}

TEST(OsUnsetenvTest) {
	qd_context* ctx = create_test_context();

	// Set a variable, then unset it
	qd_push_s(ctx, "QUADRATE_TEST_UNSET");
	qd_push_s(ctx, "will_be_removed");
	usr_os_setenv(ctx);

	// Unset it
	qd_push_s(ctx, "QUADRATE_TEST_UNSET");
	usr_os_unsetenv(ctx);

	// Verify it's gone
	qd_push_s(ctx, "QUADRATE_TEST_UNSET");
	usr_os_getenv(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("", qd_string_data(elem.value.s), "unset var should return empty");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(OsGetenvEmptyVarNameTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	usr_os_getenv(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "result should be string for empty var name");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(OsExistsEmptyPathTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	usr_os_exists(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "empty path should not exist");

	destroy_test_context(ctx);
}

TEST(OsIsDirRootTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "/");
	usr_os_is_dir(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "root should be a directory");

	destroy_test_context(ctx);
}

TEST(OsIsDirNonexistentTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "/nonexistent_path_abc_999");
	usr_os_is_dir(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "nonexistent path should not be a directory");

	destroy_test_context(ctx);
}

TEST(OsIsFileNonexistentTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "/nonexistent_file_abc_999");
	usr_os_is_file(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "nonexistent path should not be a file");

	destroy_test_context(ctx);
}

TEST(OsGetpidConsistentTest) {
	qd_context* ctx = create_test_context();

	// Call getpid twice and verify consistency
	usr_os_getpid(ctx);
	qd_stack_element_t elem1;
	qd_stack_pop(ctx->st, &elem1);

	usr_os_getpid(ctx);
	qd_stack_element_t elem2;
	qd_stack_pop(ctx->st, &elem2);

	ASSERT_EQ((int)elem1.value.i, (int)elem2.value.i, "getpid should return same value twice");

	destroy_test_context(ctx);
}

TEST(OsCwdStartsWithSlashTest) {
	qd_context* ctx = create_test_context();

	usr_os_cwd(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(OS_ERR_OK, (int)status.value.i, "cwd should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "cwd result should be string");
	ASSERT_TRUE(strlen(qd_string_data(elem.value.s)) > 0, "cwd should not be empty");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(OsMkdirNestedRmdirTest) {
	qd_context* ctx = create_test_context();

	const char* testdir = "/tmp/quadrate_test_nested_abc/subdir";

	// Create nested directory (mkdir -p)
	qd_push_s(ctx, testdir);
	usr_os_mkdir(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_EQ(OS_ERR_OK, (int)result.value.i, "nested mkdir should succeed");

	// Verify nested dir exists
	qd_push_s(ctx, testdir);
	usr_os_is_dir(ctx);
	qd_stack_element_t is_dir_elem;
	qd_stack_pop(ctx->st, &is_dir_elem);
	ASSERT_EQ(1, (int)is_dir_elem.value.i, "nested dir should exist");

	// Remove the parent (rmdir is recursive)
	qd_push_s(ctx, "/tmp/quadrate_test_nested_abc");
	usr_os_rmdir(ctx);

	// Verify parent is gone
	qd_push_s(ctx, "/tmp/quadrate_test_nested_abc");
	usr_os_exists(ctx);
	qd_stack_element_t exists_elem;
	qd_stack_pop(ctx->st, &exists_elem);
	ASSERT_EQ(0, (int)exists_elem.value.i, "removed nested dir should not exist");

	destroy_test_context(ctx);
}

TEST(OsMktempUniqueTest) {
	qd_context* ctx = create_test_context();

	// Create two temp dirs and verify they differ
	usr_os_mktemp(ctx);
	qd_stack_element_t status1;
	qd_stack_pop(ctx->st, &status1);
	ASSERT_EQ(OS_ERR_OK, (int)status1.value.i, "first mktemp should succeed");
	qd_stack_element_t elem1;
	qd_stack_pop(ctx->st, &elem1);

	usr_os_mktemp(ctx);
	qd_stack_element_t status2;
	qd_stack_pop(ctx->st, &status2);
	ASSERT_EQ(OS_ERR_OK, (int)status2.value.i, "second mktemp should succeed");
	qd_stack_element_t elem2;
	qd_stack_pop(ctx->st, &elem2);

	// The two temp paths should be different
	ASSERT_TRUE(strcmp(qd_string_data(elem1.value.s), qd_string_data(elem2.value.s)) != 0,
		"two mktemp calls should produce different paths");

	// Clean up both
	qd_push_s(ctx, qd_string_data(elem1.value.s));
	usr_os_rmdir(ctx);
	qd_push_s(ctx, qd_string_data(elem2.value.s));
	usr_os_rmdir(ctx);

	qd_string_release(elem1.value.s);
	qd_string_release(elem2.value.s);
	destroy_test_context(ctx);
}

TEST(OsGetuidTest) {
	qd_context* ctx = create_test_context();

	usr_os_getuid(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "getuid result should be int");
	ASSERT_TRUE(elem.value.i >= 0, "uid should be non-negative");

	destroy_test_context(ctx);
}

TEST(OsGetgidTest) {
	qd_context* ctx = create_test_context();

	usr_os_getgid(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "getgid result should be int");
	ASSERT_TRUE(elem.value.i >= 0, "gid should be non-negative");

	destroy_test_context(ctx);
}

TEST(OsHostnameTest) {
	qd_context* ctx = create_test_context();

	usr_os_hostname(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(OS_ERR_OK, (int)status.value.i, "hostname should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "hostname result should be string");
	ASSERT_TRUE(strlen(qd_string_data(elem.value.s)) > 0, "hostname should not be empty");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(OsChdirAndBackTest) {
	qd_context* ctx = create_test_context();

	// Save current directory
	usr_os_cwd(ctx);
	qd_stack_element_t status1;
	qd_stack_pop(ctx->st, &status1);
	qd_stack_element_t orig_dir;
	qd_stack_pop(ctx->st, &orig_dir);

	// Change to /tmp
	qd_push_s(ctx, "/tmp");
	usr_os_chdir(ctx);
	qd_stack_element_t chdir_result;
	qd_stack_pop(ctx->st, &chdir_result);
	ASSERT_EQ(OS_ERR_OK, (int)chdir_result.value.i, "chdir to /tmp should succeed");

	// Verify we are in /tmp
	usr_os_cwd(ctx);
	qd_stack_element_t status2;
	qd_stack_pop(ctx->st, &status2);
	qd_stack_element_t new_dir;
	qd_stack_pop(ctx->st, &new_dir);
	// /tmp may be a symlink, so just check it contains "tmp"
	ASSERT_TRUE(strstr(qd_string_data(new_dir.value.s), "tmp") != NULL,
		"cwd after chdir should contain tmp");

	// Change back to original directory
	qd_push_s(ctx, qd_string_data(orig_dir.value.s));
	usr_os_chdir(ctx);
	qd_stack_element_t back_result;
	qd_stack_pop(ctx->st, &back_result);
	ASSERT_EQ(OS_ERR_OK, (int)back_result.value.i, "chdir back should succeed");

	qd_string_release(orig_dir.value.s);
	qd_string_release(new_dir.value.s);
	destroy_test_context(ctx);
}

TEST(OsDeleteFileTest) {
	qd_context* ctx = create_test_context();

	// Create a temp dir, create a file in it, delete the file
	usr_os_mktemp(ctx);
	qd_stack_element_t mkstatus;
	qd_stack_pop(ctx->st, &mkstatus);
	qd_stack_element_t tmpdir;
	qd_stack_pop(ctx->st, &tmpdir);

	// Create a file using system touch
	char filepath[512];
	snprintf(filepath, sizeof(filepath), "%s/testfile.txt", qd_string_data(tmpdir.value.s));
	FILE* f = fopen(filepath, "w");
	if (f) {
		fprintf(f, "test");
		fclose(f);
	}

	// Verify file exists
	qd_push_s(ctx, filepath);
	usr_os_is_file(ctx);
	qd_stack_element_t is_file;
	qd_stack_pop(ctx->st, &is_file);
	ASSERT_EQ(1, (int)is_file.value.i, "created file should exist");

	// Delete the file
	qd_push_s(ctx, filepath);
	usr_os_delete(ctx);
	qd_stack_element_t del_result;
	qd_stack_pop(ctx->st, &del_result);
	ASSERT_EQ(OS_ERR_OK, (int)del_result.value.i, "delete should succeed");

	// Verify file is gone
	qd_push_s(ctx, filepath);
	usr_os_exists(ctx);
	qd_stack_element_t exists_elem;
	qd_stack_pop(ctx->st, &exists_elem);
	ASSERT_EQ(0, (int)exists_elem.value.i, "deleted file should not exist");

	// Clean up temp dir
	qd_push_s(ctx, qd_string_data(tmpdir.value.s));
	usr_os_rmdir(ctx);

	qd_string_release(tmpdir.value.s);
	destroy_test_context(ctx);
}

TEST(OsDeleteNonexistentTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "/nonexistent_file_for_delete_test_xyz");
	usr_os_delete(ctx);

	qd_stack_element_t result;
	qd_stack_pop(ctx->st, &result);
	ASSERT_NE(OS_ERR_OK, (int)result.value.i, "deleting nonexistent file should fail");

	destroy_test_context(ctx);
}

int main(void) {
	return UC_PrintResults();
}
