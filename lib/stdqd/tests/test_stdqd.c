#include <stdqd/fmt.h>
#include <qd/qd.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <unit-check/uc.h>

int main(void) {
	return UC_PrintResults();
}

TEST(PrintfSimple) {
	qd_context* ctx = qd_create_context(1024);

	// Test: "Hello, World!\n" qd_stdqd_printf (no arguments)
	qd_push_s(ctx, "Hello, World!\n");
	qd_exec_result result = qd_stdqd_printf(ctx);
	ASSERT_EQ(result.code, 0, "printf should succeed");

	qd_free_context(ctx);
}

TEST(PrintfWithString) {
	qd_context* ctx = qd_create_context(1024);

	// Test: "Hello, %s!\n" "Alice" fmt::printf
	qd_push_s(ctx, "Hello, %s!\n");
	qd_push_s(ctx, "Alice");
	qd_exec_result result = qd_stdqd_printf(ctx);
	ASSERT_EQ(result.code, 0, "printf should succeed");

	qd_free_context(ctx);
}

TEST(PrintfWithInt) {
	qd_context* ctx = qd_create_context(1024);

	// Test: "The answer is %d\n" 42 fmt::printf
	qd_push_s(ctx, "The answer is %d\n");
	qd_push_i(ctx, 42);
	qd_exec_result result = qd_stdqd_printf(ctx);
	ASSERT_EQ(result.code, 0, "printf should succeed");

	qd_free_context(ctx);
}

TEST(PrintfWithFloat) {
	qd_context* ctx = qd_create_context(1024);

	// Test: "Pi is approximately %f\n" 3.14159 fmt::printf
	qd_push_s(ctx, "Pi is approximately %f\n");
	qd_push_f(ctx, 3.14159);
	qd_exec_result result = qd_stdqd_printf(ctx);
	ASSERT_EQ(result.code, 0, "printf should succeed");

	qd_free_context(ctx);
}

TEST(PrintfMultipleArgs) {
	qd_context* ctx = qd_create_context(1024);

	// Test: "Name: %s, Age: %d, Height: %f m\n" "Bob" 30 1.75 fmt::printf
	qd_push_s(ctx, "Name: %s, Age: %d, Height: %f m\n");
	qd_push_s(ctx, "Bob");
	qd_push_i(ctx, 30);
	qd_push_f(ctx, 1.75);
	qd_exec_result result = qd_stdqd_printf(ctx);
	ASSERT_EQ(result.code, 0, "printf should succeed");

	qd_free_context(ctx);
}

TEST(PrintfLiteralPercent) {
	qd_context* ctx = qd_create_context(1024);

	// Test: "Progress: %d%%\n" 100 fmt::printf
	qd_push_s(ctx, "Progress: %d%%\n");
	qd_push_i(ctx, 100);
	qd_exec_result result = qd_stdqd_printf(ctx);
	ASSERT_EQ(result.code, 0, "printf should succeed");

	qd_free_context(ctx);
}
