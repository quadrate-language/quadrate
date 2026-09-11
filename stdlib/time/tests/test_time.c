/**
 * @file test_time.c
 * @brief Unit tests for the qdtime time library
 */

#include <quadrate/time/time.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/stack.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <time.h>

static qd_context* create_test_context(void) {
	return qd_create_context(256);
}

static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}

// Time constants (matching module.qd)
#define TIME_NANOSECOND  1LL
#define TIME_MICROSECOND (1000LL * TIME_NANOSECOND)
#define TIME_MILLISECOND (1000LL * TIME_MICROSECOND)
#define TIME_SECOND      (1000LL * TIME_MILLISECOND)

// Error codes (matching time.c)
#define TIME_ERR_OK     1
#define TIME_ERR_FORMAT 2
#define TIME_ERR_PARSE  3


TEST(TimeUnixTest) {
	qd_context* ctx = create_test_context();

	usr_time_unix(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "result should be int");

	// Check that timestamp is reasonable (after 2020, before 2100)
	int64_t min_timestamp = 1577836800LL;  // 2020-01-01
	int64_t max_timestamp = 4102444800LL;  // 2100-01-01
	ASSERT_TRUE(elem.value.i >= min_timestamp, "timestamp should be after 2020");
	ASSERT_TRUE(elem.value.i <= max_timestamp, "timestamp should be before 2100");

	destroy_test_context(ctx);
}

TEST(TimeUnixIncreasingTest) {
	qd_context* ctx = create_test_context();

	// Get two timestamps
	usr_time_unix(ctx);
	qd_stack_element_t elem1;
	qd_stack_pop(ctx->st, &elem1);

	usr_time_unix(ctx);
	qd_stack_element_t elem2;
	qd_stack_pop(ctx->st, &elem2);

	// Second should be >= first
	ASSERT_TRUE(elem2.value.i >= elem1.value.i, "time should not go backwards");

	destroy_test_context(ctx);
}


TEST(TimeNowTest) {
	qd_context* ctx = create_test_context();

	usr_time_now(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "result should be int");

	// Check that value is positive and reasonable
	ASSERT_TRUE(elem.value.i > 0, "nanoseconds should be positive");

	destroy_test_context(ctx);
}

TEST(TimeNowIncreasingTest) {
	qd_context* ctx = create_test_context();

	// Get two timestamps
	usr_time_now(ctx);
	qd_stack_element_t elem1;
	qd_stack_pop(ctx->st, &elem1);

	usr_time_now(ctx);
	qd_stack_element_t elem2;
	qd_stack_pop(ctx->st, &elem2);

	// Second should be >= first
	ASSERT_TRUE(elem2.value.i >= elem1.value.i, "time should not go backwards");

	destroy_test_context(ctx);
}


TEST(TimeSleepBasicTest) {
	qd_context* ctx = create_test_context();

	// Get time before
	usr_time_now(ctx);
	qd_stack_element_t before;
	qd_stack_pop(ctx->st, &before);

	// Sleep for 10 milliseconds
	qd_push_i(ctx, 10 * TIME_MILLISECOND);
	usr_time_sleep(ctx);

	// Get time after
	usr_time_now(ctx);
	qd_stack_element_t after;
	qd_stack_pop(ctx->st, &after);

	// Check that at least 5ms elapsed (allow some slack)
	int64_t elapsed = after.value.i - before.value.i;
	ASSERT_TRUE(elapsed >= 5 * TIME_MILLISECOND, "sleep should take at least 5ms");
	// But not too long (< 500ms)
	ASSERT_TRUE(elapsed < 500 * TIME_MILLISECOND, "sleep should not take > 500ms");

	destroy_test_context(ctx);
}

TEST(TimeSleepZeroTest) {
	qd_context* ctx = create_test_context();

	// Sleep for 0 nanoseconds (should return immediately)
	qd_push_i(ctx, 0);
	usr_time_sleep(ctx);

	// Just verify it didn't crash - stack should be empty
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after sleep");

	destroy_test_context(ctx);
}


TEST(TimeNowVsUnixConsistencyTest) {
	qd_context* ctx = create_test_context();

	// Get unix timestamp and nanoseconds
	usr_time_unix(ctx);
	qd_stack_element_t unix_elem;
	qd_stack_pop(ctx->st, &unix_elem);

	usr_time_now(ctx);
	qd_stack_element_t now_elem;
	qd_stack_pop(ctx->st, &now_elem);

	// Convert nanoseconds to seconds
	int64_t now_seconds = now_elem.value.i / TIME_SECOND;

	// They should be within 2 seconds of each other
	int64_t diff = unix_elem.value.i - now_seconds;
	if (diff < 0) diff = -diff;
	ASSERT_TRUE(diff <= 2, "unix and now should be consistent");

	destroy_test_context(ctx);
}

// --- Additional edge-case tests for already-tested functions ---

TEST(TimeUnixMatchesCTimeTest) {
	qd_context* ctx = create_test_context();

	time_t before = time(NULL);
	usr_time_unix(ctx);
	time_t after = time(NULL);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "result should be int");

	// The returned timestamp should be between before and after
	ASSERT_TRUE(elem.value.i >= (int64_t)before, "unix timestamp >= C time() before call");
	ASSERT_TRUE(elem.value.i <= (int64_t)after, "unix timestamp <= C time() after call");

	destroy_test_context(ctx);
}

TEST(TimeUnixStackEffectTest) {
	qd_context* ctx = create_test_context();

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should start empty");

	usr_time_unix(ctx);
	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "stack should have 1 element after unix");

	// Pop and verify stack is empty again
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after pop");

	destroy_test_context(ctx);
}

TEST(TimeNowNanosecondMagnitudeTest) {
	qd_context* ctx = create_test_context();

	usr_time_now(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "result should be int");

	// After 2024-01-01 in nanoseconds: 1704067200 * 1e9 = 1704067200000000000
	int64_t min_ns = 1704067200LL * TIME_SECOND;
	ASSERT_TRUE(elem.value.i > min_ns, "nanoseconds should correspond to date after 2024");

	// Before 2100: 4102444800 * 1e9
	int64_t max_ns = 4102444800LL * TIME_SECOND;
	ASSERT_TRUE(elem.value.i < max_ns, "nanoseconds should correspond to date before 2100");

	destroy_test_context(ctx);
}

TEST(TimeNowStackEffectTest) {
	qd_context* ctx = create_test_context();

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should start empty");

	usr_time_now(ctx);
	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "stack should have 1 element after now");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after pop");

	destroy_test_context(ctx);
}

TEST(TimeSleepSmallDurationTest) {
	qd_context* ctx = create_test_context();

	// Sleep for 1 millisecond - should complete without issue
	qd_push_i(ctx, 1 * TIME_MILLISECOND);
	usr_time_sleep(ctx);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after sleep");

	destroy_test_context(ctx);
}

TEST(TimeSleepConsumesArgumentTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "stack should have 1 element before sleep");

	usr_time_sleep(ctx);
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "sleep should consume its argument");

	destroy_test_context(ctx);
}

TEST(TimeNowElapsedDurationTest) {
	qd_context* ctx = create_test_context();

	usr_time_now(ctx);
	qd_stack_element_t t1;
	qd_stack_pop(ctx->st, &t1);

	// Do some trivial work
	for (volatile int i = 0; i < 1000; i++) {}

	usr_time_now(ctx);
	qd_stack_element_t t2;
	qd_stack_pop(ctx->st, &t2);

	int64_t elapsed = t2.value.i - t1.value.i;
	ASSERT_TRUE(elapsed >= 0, "elapsed time should be non-negative");
	// Elapsed should be less than 1 second for trivial loop
	ASSERT_TRUE(elapsed < 1 * TIME_SECOND, "elapsed should be less than 1 second");

	destroy_test_context(ctx);
}


// --- Tests for usr_time_format ---

TEST(TimeFormatBasicTest) {
	qd_context* ctx = create_test_context();

	// Format Unix epoch (0) with ISO date format
	qd_push_i(ctx, 0);
	qd_push_s(ctx, "%Y-%m-%d");
	int ret = usr_time_format(ctx);

	ASSERT_EQ(0, ret, "format should return 0 on success");

	// Stack should have: result_string, error_code(1)
	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, err_elem.type, "error code should be int");
	ASSERT_EQ(TIME_ERR_OK, (int)err_elem.value.i, "error code should be OK");

	qd_stack_element_t str_elem;
	qd_stack_pop(ctx->st, &str_elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, str_elem.type, "result should be string");

	const char* result = qd_string_data(str_elem.value.s);
	ASSERT_STR_EQ("1970-01-01", result, "epoch should format to 1970-01-01");

	qd_string_release(str_elem.value.s);
	destroy_test_context(ctx);
}

TEST(TimeFormatTimeComponentTest) {
	qd_context* ctx = create_test_context();

	// Format epoch with time format
	qd_push_i(ctx, 0);
	qd_push_s(ctx, "%H:%M:%S");
	int ret = usr_time_format(ctx);

	ASSERT_EQ(0, ret, "format should return 0 on success");

	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT_EQ(TIME_ERR_OK, (int)err_elem.value.i, "error code should be OK");

	qd_stack_element_t str_elem;
	qd_stack_pop(ctx->st, &str_elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, str_elem.type, "result should be string");

	const char* result = qd_string_data(str_elem.value.s);
	ASSERT_STR_EQ("00:00:00", result, "epoch time should be 00:00:00");

	qd_string_release(str_elem.value.s);
	destroy_test_context(ctx);
}

TEST(TimeFormatKnownTimestampTest) {
	qd_context* ctx = create_test_context();

	// 2024-06-15 12:30:45 UTC = 1718454645
	qd_push_i(ctx, 1718454645LL);
	qd_push_s(ctx, "%Y-%m-%d %H:%M:%S");
	int ret = usr_time_format(ctx);

	ASSERT_EQ(0, ret, "format should return 0 on success");

	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT_EQ(TIME_ERR_OK, (int)err_elem.value.i, "error code should be OK");

	qd_stack_element_t str_elem;
	qd_stack_pop(ctx->st, &str_elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, str_elem.type, "result should be string");

	const char* result = qd_string_data(str_elem.value.s);
	ASSERT_STR_EQ("2024-06-15 12:30:45", result, "known timestamp should format correctly");

	qd_string_release(str_elem.value.s);
	destroy_test_context(ctx);
}

TEST(TimeFormatYearOnlyTest) {
	qd_context* ctx = create_test_context();

	// 2000-01-01 00:00:00 UTC = 946684800
	qd_push_i(ctx, 946684800LL);
	qd_push_s(ctx, "%Y");
	int ret = usr_time_format(ctx);

	ASSERT_EQ(0, ret, "format should return 0 on success");

	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT_EQ(TIME_ERR_OK, (int)err_elem.value.i, "error code should be OK");

	qd_stack_element_t str_elem;
	qd_stack_pop(ctx->st, &str_elem);
	const char* result = qd_string_data(str_elem.value.s);
	ASSERT_STR_EQ("2000", result, "Y2K timestamp should give year 2000");

	qd_string_release(str_elem.value.s);
	destroy_test_context(ctx);
}

TEST(TimeFormatEmptyFormatTest) {
	qd_context* ctx = create_test_context();

	// Empty format string - strftime returns 0 for empty format with empty result
	qd_push_i(ctx, 0);
	qd_push_s(ctx, "");
	int ret = usr_time_format(ctx);

	// Empty format produces empty output; strftime returns 0 for ""
	// but the code treats len==0 with format[0]=='\0' as success
	ASSERT_EQ(0, ret, "empty format should succeed");

	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT_EQ(TIME_ERR_OK, (int)err_elem.value.i, "empty format error code should be OK");

	qd_stack_element_t str_elem;
	qd_stack_pop(ctx->st, &str_elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, str_elem.type, "result should be string");

	const char* result = qd_string_data(str_elem.value.s);
	ASSERT_STR_EQ("", result, "empty format should produce empty string");

	qd_string_release(str_elem.value.s);
	destroy_test_context(ctx);
}

TEST(TimeFormatStackEffectTest) {
	qd_context* ctx = create_test_context();

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should start empty");

	qd_push_i(ctx, 0);
	qd_push_s(ctx, "%Y");
	ASSERT_EQ(2, (int)qd_stack_size(ctx->st), "stack should have 2 elements before format");

	usr_time_format(ctx);

	// After format: result string + error code = 2 elements
	ASSERT_EQ(2, (int)qd_stack_size(ctx->st), "stack should have 2 elements after format");

	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	qd_stack_element_t str_elem;
	qd_stack_pop(ctx->st, &str_elem);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after popping results");

	qd_string_release(str_elem.value.s);
	destroy_test_context(ctx);
}

TEST(TimeFormatCurrentTimeTest) {
	qd_context* ctx = create_test_context();

	// Format current time - just verify it produces a non-empty result
	usr_time_unix(ctx);
	qd_push_s(ctx, "%F %T");
	int ret = usr_time_format(ctx);

	ASSERT_EQ(0, ret, "format current time should succeed");

	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT_EQ(TIME_ERR_OK, (int)err_elem.value.i, "error code should be OK");

	qd_stack_element_t str_elem;
	qd_stack_pop(ctx->st, &str_elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, str_elem.type, "result should be string");

	const char* result = qd_string_data(str_elem.value.s);
	ASSERT_TRUE(strlen(result) > 0, "formatted current time should not be empty");

	qd_string_release(str_elem.value.s);
	destroy_test_context(ctx);
}


// --- Tests for usr_time_parse ---

TEST(TimeParseBasicTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "2024-06-15 12:30:45");
	qd_push_s(ctx, "%Y-%m-%d %H:%M:%S");
	int ret = usr_time_parse(ctx);

	ASSERT_EQ(0, ret, "parse should return 0 on success");

	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, err_elem.type, "error code should be int");
	ASSERT_EQ(TIME_ERR_OK, (int)err_elem.value.i, "error code should be OK");

	qd_stack_element_t ts_elem;
	qd_stack_pop(ctx->st, &ts_elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, ts_elem.type, "timestamp should be int");
	ASSERT_EQ((int)1718454645LL, (int)ts_elem.value.i, "parsed timestamp should be 1718454645");

	destroy_test_context(ctx);
}

TEST(TimeParseEpochTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "1970-01-01 00:00:00");
	qd_push_s(ctx, "%Y-%m-%d %H:%M:%S");
	int ret = usr_time_parse(ctx);

	ASSERT_EQ(0, ret, "parse epoch should succeed");

	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT_EQ(TIME_ERR_OK, (int)err_elem.value.i, "error code should be OK");

	qd_stack_element_t ts_elem;
	qd_stack_pop(ctx->st, &ts_elem);
	ASSERT_EQ(0, (int)ts_elem.value.i, "epoch should parse to 0");

	destroy_test_context(ctx);
}

TEST(TimeParseDateOnlyTest) {
	qd_context* ctx = create_test_context();

	// 2000-01-01 with date-only format
	qd_push_s(ctx, "2000-01-01");
	qd_push_s(ctx, "%Y-%m-%d");
	int ret = usr_time_parse(ctx);

	ASSERT_EQ(0, ret, "parse date-only should succeed");

	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT_EQ(TIME_ERR_OK, (int)err_elem.value.i, "error code should be OK");

	qd_stack_element_t ts_elem;
	qd_stack_pop(ctx->st, &ts_elem);
	// 2000-01-01 00:00:00 UTC = 946684800
	ASSERT_EQ((int)946684800LL, (int)ts_elem.value.i, "2000-01-01 should parse to 946684800");

	destroy_test_context(ctx);
}

TEST(TimeParseInvalidInputTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "not-a-date");
	qd_push_s(ctx, "%Y-%m-%d");
	int ret = usr_time_parse(ctx);

	// Should return error
	ASSERT_EQ(TIME_ERR_PARSE, ret, "parse invalid input should return error");

	// Stack should have error code
	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, err_elem.type, "error result should be int");
	ASSERT_EQ(TIME_ERR_PARSE, (int)err_elem.value.i, "error code should be TIME_ERR_PARSE");

	destroy_test_context(ctx);
}

TEST(TimeParseStackEffectTest) {
	qd_context* ctx = create_test_context();

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should start empty");

	qd_push_s(ctx, "2024-01-01");
	qd_push_s(ctx, "%Y-%m-%d");
	ASSERT_EQ(2, (int)qd_stack_size(ctx->st), "stack should have 2 elements before parse");

	usr_time_parse(ctx);

	// After parse: timestamp + error code = 2 elements
	ASSERT_EQ(2, (int)qd_stack_size(ctx->st), "stack should have 2 elements after parse");

	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	qd_stack_element_t ts_elem;
	qd_stack_pop(ctx->st, &ts_elem);

	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after popping results");

	destroy_test_context(ctx);
}

TEST(TimeParseErrorStackEffectTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "garbage");
	qd_push_s(ctx, "%Y-%m-%d");
	usr_time_parse(ctx);

	// On error: only error code on stack (1 element)
	ASSERT_EQ(1, (int)qd_stack_size(ctx->st), "stack should have 1 element after parse error");

	qd_stack_element_t err_elem;
	qd_stack_pop(ctx->st, &err_elem);
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after popping error");

	destroy_test_context(ctx);
}


// --- Round-trip tests ---

TEST(TimeFormatParseRoundTripTest) {
	qd_context* ctx = create_test_context();

	// Format a known timestamp
	int64_t original_ts = 1718454645LL;
	qd_push_i(ctx, original_ts);
	qd_push_s(ctx, "%Y-%m-%d %H:%M:%S");
	usr_time_format(ctx);

	// Pop error code from format
	qd_stack_element_t fmt_err;
	qd_stack_pop(ctx->st, &fmt_err);
	ASSERT_EQ(TIME_ERR_OK, (int)fmt_err.value.i, "format should succeed");

	// The formatted string is on the stack; push format for parse
	qd_push_s(ctx, "%Y-%m-%d %H:%M:%S");
	usr_time_parse(ctx);

	// Pop error code from parse
	qd_stack_element_t parse_err;
	qd_stack_pop(ctx->st, &parse_err);
	ASSERT_EQ(TIME_ERR_OK, (int)parse_err.value.i, "parse should succeed");

	// Pop parsed timestamp
	qd_stack_element_t ts_elem;
	qd_stack_pop(ctx->st, &ts_elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, ts_elem.type, "result should be int");
	ASSERT_EQ((int)original_ts, (int)ts_elem.value.i, "round-trip should preserve timestamp");

	destroy_test_context(ctx);
}

TEST(TimeFormatParseRoundTripEpochTest) {
	qd_context* ctx = create_test_context();

	// Round-trip epoch
	qd_push_i(ctx, 0);
	qd_push_s(ctx, "%Y-%m-%d %H:%M:%S");
	usr_time_format(ctx);

	qd_stack_element_t fmt_err;
	qd_stack_pop(ctx->st, &fmt_err);
	ASSERT_EQ(TIME_ERR_OK, (int)fmt_err.value.i, "format epoch should succeed");

	qd_push_s(ctx, "%Y-%m-%d %H:%M:%S");
	usr_time_parse(ctx);

	qd_stack_element_t parse_err;
	qd_stack_pop(ctx->st, &parse_err);
	ASSERT_EQ(TIME_ERR_OK, (int)parse_err.value.i, "parse epoch should succeed");

	qd_stack_element_t ts_elem;
	qd_stack_pop(ctx->st, &ts_elem);
	ASSERT_EQ(0, (int)ts_elem.value.i, "round-trip epoch should return 0");

	destroy_test_context(ctx);
}

TEST(TimeMultipleFormatsTest) {
	qd_context* ctx = create_test_context();

	// Verify the same timestamp formats differently with different format strings
	int64_t ts = 946684800LL;  // 2000-01-01 00:00:00 UTC

	// Format with %F (date)
	qd_push_i(ctx, ts);
	qd_push_s(ctx, "%F");
	usr_time_format(ctx);
	qd_stack_element_t err1;
	qd_stack_pop(ctx->st, &err1);
	ASSERT_EQ(TIME_ERR_OK, (int)err1.value.i, "format %F should succeed");
	qd_stack_element_t s1;
	qd_stack_pop(ctx->st, &s1);
	ASSERT_STR_EQ("2000-01-01", qd_string_data(s1.value.s), "%F should give 2000-01-01");

	// Format with %T (time)
	qd_push_i(ctx, ts);
	qd_push_s(ctx, "%T");
	usr_time_format(ctx);
	qd_stack_element_t err2;
	qd_stack_pop(ctx->st, &err2);
	ASSERT_EQ(TIME_ERR_OK, (int)err2.value.i, "format %T should succeed");
	qd_stack_element_t s2;
	qd_stack_pop(ctx->st, &s2);
	ASSERT_STR_EQ("00:00:00", qd_string_data(s2.value.s), "%T should give 00:00:00");

	qd_string_release(s1.value.s);
	qd_string_release(s2.value.s);
	destroy_test_context(ctx);
}

int main(void) {
	return UC_PrintResults();
}
