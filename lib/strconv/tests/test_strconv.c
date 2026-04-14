/**
 * @file test_strconv.c
 * @brief Unit tests for the qdstrconv string conversion library
 */

#include <quadrate/strconv/strconv.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/stack.h>
#include <quadrate/rt/qd_string.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <string.h>

static qd_context* create_test_context(void) {
	return qd_create_context(256);
}

static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}


TEST(StrconvItoaPositiveTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	usr_strconv_itoa(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "result should be string");
	ASSERT_STR_EQ("42", qd_string_data(elem.value.s), "itoa(42)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvItoaNegativeTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, -123);
	usr_strconv_itoa(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("-123", qd_string_data(elem.value.s), "itoa(-123)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvItoaZeroTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	usr_strconv_itoa(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("0", qd_string_data(elem.value.s), "itoa(0)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


TEST(StrconvAtoiPositiveTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "42");
	usr_strconv_atoi(ctx);

	// atoi pushes [value, status] - pop status first
	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "atoi status should be Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "result should be int");
	ASSERT_EQ(42, (int)elem.value.i, "atoi(\"42\")");

	destroy_test_context(ctx);
}

TEST(StrconvAtoiNegativeTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "-123");
	usr_strconv_atoi(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(-123, (int)elem.value.i, "atoi(\"-123\")");

	destroy_test_context(ctx);
}

TEST(StrconvAtoiZeroTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "0");
	usr_strconv_atoi(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "atoi(\"0\")");

	destroy_test_context(ctx);
}


TEST(StrconvFormatIntHexTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 255);
	qd_push_i(ctx, 16);  // hex
	usr_strconv_format_int(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("ff", qd_string_data(elem.value.s), "format_int(255, 16)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatIntBinaryTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 10);
	qd_push_i(ctx, 2);  // binary
	usr_strconv_format_int(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("1010", qd_string_data(elem.value.s), "format_int(10, 2)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatIntOctalTest) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 64);
	qd_push_i(ctx, 8);  // octal
	usr_strconv_format_int(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("100", qd_string_data(elem.value.s), "format_int(64, 8)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


TEST(StrconvParseIntHexTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "ff");
	qd_push_i(ctx, 16);  // hex
	usr_strconv_parse_int(ctx);

	// parse_int pushes [value, status] - pop status first
	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(255, (int)elem.value.i, "parse_int(\"ff\", 16)");

	destroy_test_context(ctx);
}

TEST(StrconvParseIntBinaryTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "1010");
	qd_push_i(ctx, 2);  // binary
	usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(10, (int)elem.value.i, "parse_int(\"1010\", 2)");

	destroy_test_context(ctx);
}

TEST(StrconvParseIntOctalTest) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "100");
	qd_push_i(ctx, 8);  // octal
	usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(64, (int)elem.value.i, "parse_int(\"100\", 8)");

	destroy_test_context(ctx);
}


TEST(StrconvRoundtripDecimalTest) {
	qd_context* ctx = create_test_context();

	// itoa then atoi
	qd_push_i(ctx, 12345);
	usr_strconv_itoa(ctx);
	usr_strconv_atoi(ctx);

	// pop atoi status
	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(12345, (int)elem.value.i, "roundtrip decimal");

	destroy_test_context(ctx);
}

TEST(StrconvRoundtripHexTest) {
	qd_context* ctx = create_test_context();

	// format_int then parse_int
	qd_push_i(ctx, 0xABCD);
	qd_push_i(ctx, 16);
	usr_strconv_format_int(ctx);

	qd_push_i(ctx, 16);
	usr_strconv_parse_int(ctx);

	// pop parse_int status
	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0xABCD, (int)elem.value.i, "roundtrip hex");

	destroy_test_context(ctx);
}

int main(void) {
	return UC_PrintResults();
}
