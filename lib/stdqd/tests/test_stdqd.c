#include <stdqd/stdqd.h>
#include <quadrate/qd.h>
#include <quadrate/runtime/runtime.h>
#include <quadrate/runtime/stack.h>
#include <unit-check/uc.h>

int main(void) {
	return UC_PrintResults();
}

TEST(PrintfSimple) {
	qd_context* ctx = qd_create_context(1024);

	// Test: "Hello, World!\n" qd_stdqd_printf
	qd_push_s(ctx, "Hello, World!\n");
	qd_exec_result result = qd_stdqd_printf(ctx);
	ASSERT_EQ(result.code, 0, "printf should succeed");

	qd_free_context(ctx);
}

TEST(PrintfWithString) {
	qd_context* ctx = qd_create_context(1024);

	// Test: "Alice" "Hello, %s!\n" qd_stdqd_printf
	qd_push_s(ctx, "Alice");
	qd_push_s(ctx, "Hello, %s!\n");
	qd_exec_result result = qd_stdqd_printf(ctx);
	ASSERT_EQ(result.code, 0, "printf should succeed");

	qd_free_context(ctx);
}

TEST(PrintfWithInt) {
	qd_context* ctx = qd_create_context(1024);

	// Test: 42 "The answer is %d\n" qd_stdqd_printf
	qd_push_i(ctx, 42);
	qd_push_s(ctx, "The answer is %d\n");
	qd_exec_result result = qd_stdqd_printf(ctx);
	ASSERT_EQ(result.code, 0, "printf should succeed");

	qd_free_context(ctx);
}

TEST(PrintfWithFloat) {
	qd_context* ctx = qd_create_context(1024);

	// Test: 3.14159 "Pi is approximately %f\n" qd_stdqd_printf
	qd_push_f(ctx, 3.14159);
	qd_push_s(ctx, "Pi is approximately %f\n");
	qd_exec_result result = qd_stdqd_printf(ctx);
	ASSERT_EQ(result.code, 0, "printf should succeed");

	qd_free_context(ctx);
}

TEST(PrintfMultipleArgs) {
	qd_context* ctx = qd_create_context(1024);

	// Test: "Bob" 30 1.75 "Name: %s, Age: %d, Height: %f m\n" qd_stdqd_printf
	qd_push_s(ctx, "Bob");
	qd_push_i(ctx, 30);
	qd_push_f(ctx, 1.75);
	qd_push_s(ctx, "Name: %s, Age: %d, Height: %f m\n");
	qd_exec_result result = qd_stdqd_printf(ctx);
	ASSERT_EQ(result.code, 0, "printf should succeed");

	qd_free_context(ctx);
}

TEST(PrintfLiteralPercent) {
	qd_context* ctx = qd_create_context(1024);

	// Test: 100 "Progress: %d%%\n" qd_stdqd_printf
	qd_push_i(ctx, 100);
	qd_push_s(ctx, "Progress: %d%%\n");
	qd_exec_result result = qd_stdqd_printf(ctx);
	ASSERT_EQ(result.code, 0, "printf should succeed");

	qd_free_context(ctx);
}
