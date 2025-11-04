#include <stdqd/stdqd.h>
#include <quadrate/qd.h>
#include <quadrate/runtime/runtime.h>
#include <quadrate/runtime/stack.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

void test_printf_simple(void) {
	printf("\n=== test_printf_simple ===\n");

	qd_context* ctx = qd_create_context(1024);
	assert(ctx != NULL);

	// Test: "Hello, World!\n" qd_stdqd_printf
	qd_push_s(ctx, "Hello, World!\n");
	qd_stdqd_printf(ctx);

	qd_free_context(ctx);
	printf("✓ Simple printf test passed\n");
}

void test_printf_with_string(void) {
	printf("\n=== test_printf_with_string ===\n");

	qd_context* ctx = qd_create_context(1024);
	assert(ctx != NULL);

	// Test: "Alice" "Hello, %s!\n" qd_stdqd_printf
	qd_push_s(ctx, "Alice");
	qd_push_s(ctx, "Hello, %s!\n");
	qd_stdqd_printf(ctx);

	qd_free_context(ctx);
	printf("✓ Printf with string test passed\n");
}

void test_printf_with_int(void) {
	printf("\n=== test_printf_with_int ===\n");

	qd_context* ctx = qd_create_context(1024);
	assert(ctx != NULL);

	// Test: 42 "The answer is %d\n" qd_stdqd_printf
	qd_push_i(ctx, 42);
	qd_push_s(ctx, "The answer is %d\n");
	qd_stdqd_printf(ctx);

	qd_free_context(ctx);
	printf("✓ Printf with int test passed\n");
}

void test_printf_with_float(void) {
	printf("\n=== test_printf_with_float ===\n");

	qd_context* ctx = qd_create_context(1024);
	assert(ctx != NULL);

	// Test: 3.14159 "Pi is approximately %f\n" qd_stdqd_printf
	qd_push_f(ctx, 3.14159);
	qd_push_s(ctx, "Pi is approximately %f\n");
	qd_stdqd_printf(ctx);

	qd_free_context(ctx);
	printf("✓ Printf with float test passed\n");
}

void test_printf_multiple_args(void) {
	printf("\n=== test_printf_multiple_args ===\n");

	qd_context* ctx = qd_create_context(1024);
	assert(ctx != NULL);

	// Test: "Bob" 30 3.5 "Name: %s, Age: %d, Height: %f m\n" qd_stdqd_printf
	qd_push_s(ctx, "Bob");
	qd_push_i(ctx, 30);
	qd_push_f(ctx, 1.75);
	qd_push_s(ctx, "Name: %s, Age: %d, Height: %f m\n");
	qd_stdqd_printf(ctx);

	qd_free_context(ctx);
	printf("✓ Printf with multiple args test passed\n");
}

void test_printf_literal_percent(void) {
	printf("\n=== test_printf_literal_percent ===\n");

	qd_context* ctx = qd_create_context(1024);
	assert(ctx != NULL);

	// Test: 100 "Progress: %d%%\n" qd_stdqd_printf
	qd_push_i(ctx, 100);
	qd_push_s(ctx, "Progress: %d%%\n");
	qd_stdqd_printf(ctx);

	qd_free_context(ctx);
	printf("✓ Printf with literal %% test passed\n");
}

int main(void) {
	printf("Running libstdqd tests...\n");

	test_printf_simple();
	test_printf_with_string();
	test_printf_with_int();
	test_printf_with_float();
	test_printf_multiple_args();
	test_printf_literal_percent();

	printf("\n===================\n");
	printf("All tests passed!\n");
	printf("===================\n");
	return 0;
}
