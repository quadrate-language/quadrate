/**
 * @file test_time.c
 * @brief Unit tests for the qdtime time library
 */

#include <qdtime/time.h>
#include <qdrt/runtime.h>
#include <qdrt/context.h>
#include <qdrt/stack.h>
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

int main(void) {
	return UC_PrintResults();
}
