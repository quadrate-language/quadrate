#include <stdqd/fmt.h>
#include <stdqd/math.h>
#include <stdqd/str.h>
#include <stdqd/bits.h>
#include <stdqd/time.h>
#include <qd/qd.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <unit-check/uc.h>
#include <math.h>
#include <string.h>

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

// Math tests
TEST(MathSqrt) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_f(ctx, 16.0);
	qd_exec_result result = qd_stdqd_sqrt(ctx);
	ASSERT_EQ(result.code, 0, "sqrt should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT_EQ((int)elem.value.f, 4, "sqrt(16) should be 4");

	qd_free_context(ctx);
}

TEST(MathPow) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_f(ctx, 2.0);
	qd_push_f(ctx, 8.0);
	qd_exec_result result = qd_stdqd_pow(ctx);
	ASSERT_EQ(result.code, 0, "pow should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT_EQ((int)elem.value.f, 256, "2^8 should be 256");

	qd_free_context(ctx);
}

TEST(MathSin) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_f(ctx, 0.0);
	qd_exec_result result = qd_stdqd_sin(ctx);
	ASSERT_EQ(result.code, 0, "sin should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT_EQ((int)elem.value.f, 0, "sin(0) should be 0");

	qd_free_context(ctx);
}

TEST(MathAbs) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_f(ctx, -5.5);
	qd_exec_result result = qd_stdqd_abs(ctx);
	ASSERT_EQ(result.code, 0, "abs should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	double diff = elem.value.f - 5.5;
	if (diff < 0) diff = -diff;
	ASSERT_TRUE(diff < 0.01, "abs(-5.5) should be 5.5");

	qd_free_context(ctx);
}

TEST(MathCeil) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_f(ctx, 3.2);
	qd_exec_result result = qd_stdqd_ceil(ctx);
	ASSERT_EQ(result.code, 0, "ceil should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT_EQ((int)elem.value.f, 4, "ceil(3.2) should be 4");

	qd_free_context(ctx);
}

TEST(MathFloor) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_f(ctx, 3.8);
	qd_exec_result result = qd_stdqd_floor(ctx);
	ASSERT_EQ(result.code, 0, "floor should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_FLOAT, "result should be float");
	ASSERT_EQ((int)elem.value.f, 3, "floor(3.8) should be 3");

	qd_free_context(ctx);
}

TEST(MathMinMax) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_f(ctx, 3.0);
	qd_push_f(ctx, 7.0);
	qd_exec_result result = qd_stdqd_min(ctx);
	ASSERT_EQ(result.code, 0, "min should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.f, 3, "min(3, 7) should be 3");

	qd_push_f(ctx, 3.0);
	qd_push_f(ctx, 7.0);
	result = qd_stdqd_max(ctx);
	ASSERT_EQ(result.code, 0, "max should succeed");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.f, 7, "max(3, 7) should be 7");

	qd_free_context(ctx);
}

// String tests
TEST(StrLen) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_s(ctx, "Hello");
	qd_exec_result result = qd_stdqd_len(ctx);
	ASSERT_EQ(result.code, 0, "len should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ(elem.value.i, 5, "len('Hello') should be 5");

	qd_free_context(ctx);
}

TEST(StrConcat) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_s(ctx, "Hello, ");
	qd_push_s(ctx, "World!");
	qd_exec_result result = qd_stdqd_concat(ctx);
	ASSERT_EQ(result.code, 0, "concat should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "result should be string");
	ASSERT_STR_EQ(elem.value.s, "Hello, World!", "concat should work");

	qd_free_context(ctx);
}

TEST(StrUpper) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_s(ctx, "hello");
	qd_exec_result result = qd_stdqd_upper(ctx);
	ASSERT_EQ(result.code, 0, "upper should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "result should be string");
	ASSERT_STR_EQ(elem.value.s, "HELLO", "upper should work");

	qd_free_context(ctx);
}

TEST(StrLower) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_s(ctx, "WORLD");
	qd_exec_result result = qd_stdqd_lower(ctx);
	ASSERT_EQ(result.code, 0, "lower should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "result should be string");
	ASSERT_STR_EQ(elem.value.s, "world", "lower should work");

	qd_free_context(ctx);
}

TEST(StrTrim) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_s(ctx, "  hello  ");
	qd_exec_result result = qd_stdqd_trim(ctx);
	ASSERT_EQ(result.code, 0, "trim should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_STR, "result should be string");
	ASSERT_STR_EQ(elem.value.s, "hello", "trim should remove whitespace");

	qd_free_context(ctx);
}

TEST(StrContains) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_s(ctx, "Hello World");
	qd_push_s(ctx, "World");
	qd_exec_result result = qd_stdqd_contains(ctx);
	ASSERT_EQ(result.code, 0, "contains should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_NE(elem.value.i, 0, "should contain 'World'");

	qd_free_context(ctx);
}

TEST(StrStartsWith) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_s(ctx, "Hello World");
	qd_push_s(ctx, "Hello");
	qd_exec_result result = qd_stdqd_starts_with(ctx);
	ASSERT_EQ(result.code, 0, "starts_with should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_NE(elem.value.i, 0, "should start with 'Hello'");

	qd_free_context(ctx);
}

TEST(StrEndsWith) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_s(ctx, "Hello World");
	qd_push_s(ctx, "World");
	qd_exec_result result = qd_stdqd_ends_with(ctx);
	ASSERT_EQ(result.code, 0, "ends_with should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_NE(elem.value.i, 0, "should end with 'World'");

	qd_free_context(ctx);
}

// Bits tests
TEST(BitsAnd) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_i(ctx, 12);  // 0b1100
	qd_push_i(ctx, 10);  // 0b1010
	qd_exec_result result = qd_stdqd_and(ctx);
	ASSERT_EQ(result.code, 0, "and should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ(elem.value.i, 8, "12 & 10 should be 8");

	qd_free_context(ctx);
}

TEST(BitsOr) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_i(ctx, 12);  // 0b1100
	qd_push_i(ctx, 10);  // 0b1010
	qd_exec_result result = qd_stdqd_or(ctx);
	ASSERT_EQ(result.code, 0, "or should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ(elem.value.i, 14, "12 | 10 should be 14");

	qd_free_context(ctx);
}

TEST(BitsXor) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_i(ctx, 12);  // 0b1100
	qd_push_i(ctx, 10);  // 0b1010
	qd_exec_result result = qd_stdqd_xor(ctx);
	ASSERT_EQ(result.code, 0, "xor should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ(elem.value.i, 6, "12 ^ 10 should be 6");

	qd_free_context(ctx);
}

TEST(BitsNot) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_i(ctx, 5);
	qd_exec_result result = qd_stdqd_not(ctx);
	ASSERT_EQ(result.code, 0, "not should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.type, QD_STACK_TYPE_INT, "result should be int");
	ASSERT_EQ(elem.value.i, -6, "~5 should be -6");

	qd_free_context(ctx);
}

TEST(BitsShift) {
	qd_context* ctx = qd_create_context(1024);

	qd_push_i(ctx, 4);
	qd_push_i(ctx, 2);
	qd_exec_result result = qd_stdqd_lshift(ctx);
	ASSERT_EQ(result.code, 0, "lshift should succeed");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.value.i, 16, "4 << 2 should be 16");

	qd_push_i(ctx, 16);
	qd_push_i(ctx, 2);
	result = qd_stdqd_rshift(ctx);
	ASSERT_EQ(result.code, 0, "rshift should succeed");

	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(elem.value.i, 4, "16 >> 2 should be 4");

	qd_free_context(ctx);
}

// Time tests
TEST(TimeSleep) {
	qd_context* ctx = qd_create_context(1024);

	// Test very short sleep (1 millisecond)
	qd_push_i(ctx, 1);
	qd_exec_result result = qd_stdqd_sleep(ctx);
	ASSERT_EQ(result.code, 0, "sleep should succeed");

	qd_free_context(ctx);
}
