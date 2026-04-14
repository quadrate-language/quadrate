/**
 * @file test_log.c
 * @brief Unit tests for the qdlog logging library
 */

#define _POSIX_C_SOURCE 200809L

#include <quadrate/log/log.h>
#include <unit-check/uc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/* ---------- helpers ---------- */

static char* make_tmpfile(void) {
	char* path = strdup("/tmp/qdlog_test_XXXXXX");
	int fd = mkstemp(path);
	if (fd < 0) {
		free(path);
		return NULL;
	}
	close(fd);
	return path;
}

static char* read_file_contents(const char* path) {
	FILE* f = fopen(path, "r");
	if (!f) return NULL;

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* buf = malloc((size_t)len + 1);
	if (!buf) { fclose(f); return NULL; }

	size_t nread = fread(buf, 1, (size_t)len, f);
	buf[nread] = '\0';
	fclose(f);
	return buf;
}

static int64_t file_size(const char* path) {
	struct stat st;
	if (stat(path, &st) == 0) return st.st_size;
	return -1;
}

/* ---------- 1. create / free ---------- */

TEST(LogCreateFreeTest) {
	qdlog_logger_t* logger = qdlog_new();
	ASSERT(logger != NULL, "logger should not be null");
	qdlog_free(logger);
}

/* ---------- 2. set / get level roundtrip ---------- */

TEST(LogSetGetLevelTest) {
	qdlog_logger_t* logger = qdlog_new();

	qdlog_set_level(logger, QDLOG_LEVEL_DEBUG);
	ASSERT_EQ((int)QDLOG_LEVEL_DEBUG, (int)qdlog_get_level(logger), "level should be DEBUG");

	qdlog_set_level(logger, QDLOG_LEVEL_WARN);
	ASSERT_EQ((int)QDLOG_LEVEL_WARN, (int)qdlog_get_level(logger), "level should be WARN");

	qdlog_set_level(logger, QDLOG_LEVEL_ERROR);
	ASSERT_EQ((int)QDLOG_LEVEL_ERROR, (int)qdlog_get_level(logger), "level should be ERROR");

	qdlog_set_level(logger, QDLOG_LEVEL_OFF);
	ASSERT_EQ((int)QDLOG_LEVEL_OFF, (int)qdlog_get_level(logger), "level should be OFF");

	qdlog_free(logger);
}

/* ---------- 3. level filtering ---------- */

TEST(LogLevelFilteringTest) {
	qdlog_logger_t* logger = qdlog_new();
	qdlog_disable_stdout(logger);

	char* path = make_tmpfile();
	ASSERT(path != NULL, "tmpfile creation should succeed");

	int64_t rc = qdlog_add_file(logger, path);
	ASSERT_EQ(0, (int)rc, "add_file should succeed");

	/* Set level to INFO -- DEBUG messages should be suppressed */
	qdlog_set_level(logger, QDLOG_LEVEL_INFO);
	qdlog_log(logger, QDLOG_LEVEL_DEBUG, "debug_msg_should_not_appear");
	qdlog_log(logger, QDLOG_LEVEL_INFO, "info_msg_should_appear");
	qdlog_flush(logger);

	char* content = read_file_contents(path);
	ASSERT(content != NULL, "should be able to read log file");
	ASSERT(strstr(content, "debug_msg_should_not_appear") == NULL,
	       "DEBUG message should be suppressed when level is INFO");
	ASSERT(strstr(content, "info_msg_should_appear") != NULL,
	       "INFO message should appear when level is INFO");

	free(content);
	unlink(path);
	free(path);
	qdlog_free(logger);
}

/* ---------- 4. set format ---------- */

TEST(LogSetFormatTextTest) {
	qdlog_logger_t* logger = qdlog_new();
	qdlog_disable_stdout(logger);

	char* path = make_tmpfile();
	ASSERT(path != NULL, "tmpfile creation should succeed");

	qdlog_set_format(logger, QDLOG_FORMAT_TEXT);
	int64_t rc = qdlog_add_file(logger, path);
	ASSERT_EQ(0, (int)rc, "add_file should succeed");

	qdlog_log(logger, QDLOG_LEVEL_INFO, "text_format_msg");
	qdlog_flush(logger);

	char* content = read_file_contents(path);
	ASSERT(content != NULL, "should read file");
	ASSERT(strstr(content, "[INFO") != NULL, "TEXT format should contain [INFO]");
	ASSERT(strstr(content, "text_format_msg") != NULL, "message should appear");

	free(content);
	unlink(path);
	free(path);
	qdlog_free(logger);
}

TEST(LogSetFormatJsonTest) {
	qdlog_logger_t* logger = qdlog_new();
	qdlog_disable_stdout(logger);

	char* path = make_tmpfile();
	ASSERT(path != NULL, "tmpfile creation should succeed");

	qdlog_set_format(logger, QDLOG_FORMAT_JSON);
	int64_t rc = qdlog_add_file(logger, path);
	ASSERT_EQ(0, (int)rc, "add_file should succeed");

	qdlog_log(logger, QDLOG_LEVEL_WARN, "json_format_msg");
	qdlog_flush(logger);

	char* content = read_file_contents(path);
	ASSERT(content != NULL, "should read file");
	ASSERT(strstr(content, "\"level\":\"warn\"") != NULL,
	       "JSON format should contain level field");
	ASSERT(strstr(content, "\"msg\":\"json_format_msg\"") != NULL,
	       "JSON format should contain msg field");

	free(content);
	unlink(path);
	free(path);
	qdlog_free(logger);
}

/* ---------- 5. add file output ---------- */

TEST(LogAddFileTest) {
	qdlog_logger_t* logger = qdlog_new();
	qdlog_disable_stdout(logger);

	char* path = make_tmpfile();
	ASSERT(path != NULL, "tmpfile creation should succeed");

	int64_t rc = qdlog_add_file(logger, path);
	ASSERT_EQ(0, (int)rc, "add_file should return QDLOG_OK");

	qdlog_log(logger, QDLOG_LEVEL_INFO, "file_output_test");
	qdlog_flush(logger);

	char* content = read_file_contents(path);
	ASSERT(content != NULL, "should be able to read the log file");
	ASSERT(strlen(content) > 0, "log file should not be empty");

	free(content);
	unlink(path);
	free(path);
	qdlog_free(logger);
}

/* ---------- 6. log to file -- verify content ---------- */

TEST(LogToFileContentTest) {
	qdlog_logger_t* logger = qdlog_new();
	qdlog_disable_stdout(logger);

	char* path = make_tmpfile();
	ASSERT(path != NULL, "tmpfile creation should succeed");

	int64_t rc = qdlog_add_file(logger, path);
	ASSERT_EQ(0, (int)rc, "add_file should succeed");

	qdlog_set_level(logger, QDLOG_LEVEL_DEBUG);
	qdlog_log(logger, QDLOG_LEVEL_ERROR, "critical_failure_xyz");
	qdlog_flush(logger);

	char* content = read_file_contents(path);
	ASSERT(content != NULL, "should read file");
	ASSERT(strstr(content, "critical_failure_xyz") != NULL,
	       "message text should appear in log file");
	ASSERT(strstr(content, "ERROR") != NULL,
	       "level name should appear in log file");

	free(content);
	unlink(path);
	free(path);
	qdlog_free(logger);
}

/* ---------- 7. log_kv structured output ---------- */

TEST(LogKvTextTest) {
	qdlog_logger_t* logger = qdlog_new();
	qdlog_disable_stdout(logger);

	char* path = make_tmpfile();
	ASSERT(path != NULL, "tmpfile creation should succeed");

	qdlog_set_format(logger, QDLOG_FORMAT_TEXT);
	int64_t rc = qdlog_add_file(logger, path);
	ASSERT_EQ(0, (int)rc, "add_file should succeed");

	const char* pairs[] = { "user", "alice", "action", "login" };
	qdlog_log_kv(logger, QDLOG_LEVEL_INFO, "kv_test_msg", pairs, 2);
	qdlog_flush(logger);

	char* content = read_file_contents(path);
	ASSERT(content != NULL, "should read file");
	ASSERT(strstr(content, "kv_test_msg") != NULL, "message should appear");
	ASSERT(strstr(content, "user=alice") != NULL, "key-value user=alice should appear");
	ASSERT(strstr(content, "action=login") != NULL, "key-value action=login should appear");

	free(content);
	unlink(path);
	free(path);
	qdlog_free(logger);
}

TEST(LogKvJsonTest) {
	qdlog_logger_t* logger = qdlog_new();
	qdlog_disable_stdout(logger);

	char* path = make_tmpfile();
	ASSERT(path != NULL, "tmpfile creation should succeed");

	qdlog_set_format(logger, QDLOG_FORMAT_JSON);
	int64_t rc = qdlog_add_file(logger, path);
	ASSERT_EQ(0, (int)rc, "add_file should succeed");

	const char* pairs[] = { "host", "server01", "port", "8080" };
	qdlog_log_kv(logger, QDLOG_LEVEL_INFO, "kv_json_msg", pairs, 2);
	qdlog_flush(logger);

	char* content = read_file_contents(path);
	ASSERT(content != NULL, "should read file");
	ASSERT(strstr(content, "\"host\":\"server01\"") != NULL,
	       "JSON should contain host key-value");
	ASSERT(strstr(content, "\"port\":\"8080\"") != NULL,
	       "JSON should contain port key-value");

	free(content);
	unlink(path);
	free(path);
	qdlog_free(logger);
}

/* ---------- 8. disable / enable stdout ---------- */

TEST(LogDisableEnableStdoutTest) {
	qdlog_logger_t* logger = qdlog_new();

	/* These should not crash -- we cannot easily capture stdout here,
	   but we verify the calls succeed and the logger remains usable. */
	qdlog_disable_stdout(logger);
	qdlog_log(logger, QDLOG_LEVEL_INFO, "should_not_print_to_stdout");

	qdlog_enable_stdout(logger);
	qdlog_log(logger, QDLOG_LEVEL_INFO, "should_print_to_stdout");

	/* Logger still works after toggling */
	ASSERT_EQ((int)QDLOG_LEVEL_INFO, (int)qdlog_get_level(logger),
	          "logger should still be functional after stdout toggle");

	qdlog_free(logger);
}

/* ---------- 9. flush does not crash ---------- */

TEST(LogFlushNoCrashTest) {
	qdlog_logger_t* logger = qdlog_new();
	qdlog_disable_stdout(logger);

	/* Flush with no file outputs */
	qdlog_flush(logger);

	/* Add a file and flush again */
	char* path = make_tmpfile();
	ASSERT(path != NULL, "tmpfile creation should succeed");

	int64_t rc = qdlog_add_file(logger, path);
	ASSERT_EQ(0, (int)rc, "add_file should succeed");

	qdlog_log(logger, QDLOG_LEVEL_INFO, "before_flush");
	qdlog_flush(logger);

	/* Flush with NULL logger should not crash */
	qdlog_flush(NULL);

	ASSERT(1, "flush operations did not crash");

	unlink(path);
	free(path);
	qdlog_free(logger);
}

/* ---------- 10. add file with invalid path ---------- */

TEST(LogAddFileInvalidPathTest) {
	qdlog_logger_t* logger = qdlog_new();

	int64_t rc = qdlog_add_file(logger, "/nonexistent/dir/that/does/not/exist/log.txt");
	ASSERT_NE(0, (int)rc, "add_file with invalid path should return error");

	/* NULL path */
	int64_t rc2 = qdlog_add_file(logger, NULL);
	ASSERT_NE(0, (int)rc2, "add_file with NULL path should return error");

	qdlog_free(logger);
}

/* ---------- 11. file rotation by size ---------- */

TEST(LogFileRotationBySizeTest) {
	qdlog_logger_t* logger = qdlog_new();
	qdlog_disable_stdout(logger);
	qdlog_set_level(logger, QDLOG_LEVEL_DEBUG);

	char* path = make_tmpfile();
	ASSERT(path != NULL, "tmpfile creation should succeed");

	/* Small max_size to trigger rotation quickly. max_files = 3 */
	int64_t max_size = 100;
	int64_t rc = qdlog_add_file_rotate(logger, path, QDLOG_ROTATE_SIZE, max_size, 3);
	ASSERT_EQ(0, (int)rc, "add_file_rotate should succeed");

	/* Write enough data to exceed max_size and trigger rotation.
	   Each log line is roughly 50-80 bytes, so a few writes should do it. */
	for (int i = 0; i < 10; i++) {
		qdlog_log(logger, QDLOG_LEVEL_INFO, "rotation_padding_message_for_size_test");
	}
	qdlog_flush(logger);

	/* After rotation, the rotated file .1 should exist */
	char rotated[1024];
	snprintf(rotated, sizeof(rotated), "%s.1", path);

	int64_t rotated_size = file_size(rotated);
	ASSERT(rotated_size > 0, "rotated file .1 should exist and have content");

	/* Clean up: remove base file and rotated files */
	unlink(path);
	for (int i = 1; i <= 4; i++) {
		char rpath[1024];
		snprintf(rpath, sizeof(rpath), "%s.%d", path, i);
		unlink(rpath);
	}

	free(path);
	qdlog_free(logger);
}

int main(void) {
	return UC_PrintResults();
}
