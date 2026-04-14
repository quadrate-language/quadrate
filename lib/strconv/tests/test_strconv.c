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
#include <stdint.h>
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

/* ===== itoa edge cases ===== */

TEST(StrconvItoaLargePositive) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 999999999);
	usr_strconv_itoa(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("999999999", qd_string_data(elem.value.s), "itoa(999999999)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvItoaLargeNegative) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, -999999999);
	usr_strconv_itoa(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("-999999999", qd_string_data(elem.value.s), "itoa(-999999999)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvItoaInt64Max) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, INT64_MAX);
	usr_strconv_itoa(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("9223372036854775807", qd_string_data(elem.value.s), "itoa(INT64_MAX)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvItoaInt64Min) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, INT64_MIN);
	usr_strconv_itoa(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("-9223372036854775808", qd_string_data(elem.value.s), "itoa(INT64_MIN)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvItoaOne) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 1);
	usr_strconv_itoa(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("1", qd_string_data(elem.value.s), "itoa(1)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvItoaMinusOne) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, -1);
	usr_strconv_itoa(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("-1", qd_string_data(elem.value.s), "itoa(-1)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ===== atoi edge cases ===== */

TEST(StrconvAtoiLeadingWhitespace) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "  42");
	usr_strconv_atoi(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "atoi whitespace status should be Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(42, (int)elem.value.i, "atoi(\"  42\") should handle leading whitespace");

	destroy_test_context(ctx);
}

TEST(StrconvAtoiEmptyStringError) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	int rc = usr_strconv_atoi(ctx);

	/* On error, only a single status value is pushed */
	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(2, (int)status.value.i, "atoi(\"\") should return error status");
	ASSERT_NE(0, rc, "atoi(\"\") should return non-zero rc");

	destroy_test_context(ctx);
}

TEST(StrconvAtoiNonNumericError) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "abc");
	int rc = usr_strconv_atoi(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(2, (int)status.value.i, "atoi(\"abc\") should return error status");
	ASSERT_NE(0, rc, "atoi(\"abc\") should return non-zero rc");

	destroy_test_context(ctx);
}

TEST(StrconvAtoiLargeNumber) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "1000000");
	usr_strconv_atoi(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "atoi large number status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1000000, (int)elem.value.i, "atoi(\"1000000\")");

	destroy_test_context(ctx);
}

TEST(StrconvAtoiPositiveSign) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "+99");
	usr_strconv_atoi(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "atoi(\"+99\") status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(99, (int)elem.value.i, "atoi(\"+99\") should parse positive sign");

	destroy_test_context(ctx);
}


/* ===== format_int additional cases ===== */

TEST(StrconvFormatIntDecimal) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 12345);
	qd_push_i(ctx, 10);
	usr_strconv_format_int(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("12345", qd_string_data(elem.value.s), "format_int(12345, 10)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatIntZero) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	qd_push_i(ctx, 16);
	usr_strconv_format_int(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("0", qd_string_data(elem.value.s), "format_int(0, 16) should be \"0\"");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatIntNegativeHex) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, -255);
	qd_push_i(ctx, 16);
	usr_strconv_format_int(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("-ff", qd_string_data(elem.value.s), "format_int(-255, 16)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatIntNegativeBinary) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, -10);
	qd_push_i(ctx, 2);
	usr_strconv_format_int(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("-1010", qd_string_data(elem.value.s), "format_int(-10, 2)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatIntHexLargeValue) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0xDEADBEEF);
	qd_push_i(ctx, 16);
	usr_strconv_format_int(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("deadbeef", qd_string_data(elem.value.s), "format_int(0xDEADBEEF, 16)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatIntBase36) {
	qd_context* ctx = create_test_context();

	/* 35 in base 36 is 'z' */
	qd_push_i(ctx, 35);
	qd_push_i(ctx, 36);
	usr_strconv_format_int(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("z", qd_string_data(elem.value.s), "format_int(35, 36)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatIntBinaryOne) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 1);
	qd_push_i(ctx, 2);
	usr_strconv_format_int(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("1", qd_string_data(elem.value.s), "format_int(1, 2)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatIntBinaryPowerOfTwo) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 256);
	qd_push_i(ctx, 2);
	usr_strconv_format_int(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("100000000", qd_string_data(elem.value.s), "format_int(256, 2)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ===== parse_int additional cases ===== */

TEST(StrconvParseIntDecimal) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "12345");
	qd_push_i(ctx, 10);
	usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_int decimal status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(12345, (int)elem.value.i, "parse_int(\"12345\", 10)");

	destroy_test_context(ctx);
}

TEST(StrconvParseIntLeadingWhitespace) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "  ff");
	qd_push_i(ctx, 16);
	usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_int whitespace status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(255, (int)elem.value.i, "parse_int(\"  ff\", 16) should skip whitespace");

	destroy_test_context(ctx);
}

TEST(StrconvParseIntNegativeDecimal) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "-42");
	qd_push_i(ctx, 10);
	usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_int negative status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(-42, (int)elem.value.i, "parse_int(\"-42\", 10)");

	destroy_test_context(ctx);
}

TEST(StrconvParseIntPositiveSign) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "+77");
	qd_push_i(ctx, 10);
	usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_int positive sign status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(77, (int)elem.value.i, "parse_int(\"+77\", 10)");

	destroy_test_context(ctx);
}

TEST(StrconvParseIntInvalidError) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "xyz");
	qd_push_i(ctx, 10);
	int rc = usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(2, (int)status.value.i, "parse_int(\"xyz\", 10) should return error status");
	ASSERT_NE(0, rc, "parse_int(\"xyz\", 10) should return non-zero rc");

	destroy_test_context(ctx);
}

TEST(StrconvParseIntEmptyError) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	qd_push_i(ctx, 10);
	int rc = usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(2, (int)status.value.i, "parse_int(\"\", 10) should return error status");
	ASSERT_NE(0, rc, "parse_int(\"\", 10) should return non-zero rc");

	destroy_test_context(ctx);
}

TEST(StrconvParseIntUppercaseHex) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "FF");
	qd_push_i(ctx, 16);
	usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_int uppercase hex status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(255, (int)elem.value.i, "parse_int(\"FF\", 16) uppercase");

	destroy_test_context(ctx);
}

TEST(StrconvParseIntZero) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "0");
	qd_push_i(ctx, 10);
	usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_int zero status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "parse_int(\"0\", 10)");

	destroy_test_context(ctx);
}

TEST(StrconvParseIntBase36) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "z");
	qd_push_i(ctx, 36);
	usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_int base36 status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(35, (int)elem.value.i, "parse_int(\"z\", 36) should be 35");

	destroy_test_context(ctx);
}

TEST(StrconvParseIntInvalidDigitForBase) {
	qd_context* ctx = create_test_context();

	/* '2' is not a valid binary digit */
	qd_push_s(ctx, "2");
	qd_push_i(ctx, 2);
	int rc = usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(2, (int)status.value.i, "parse_int(\"2\", 2) should return error status");
	ASSERT_NE(0, rc, "parse_int(\"2\", 2) should return non-zero rc");

	destroy_test_context(ctx);
}


/* ===== format_float tests ===== */

TEST(StrconvFormatFloatPositive) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.14);
	usr_strconv_format_float(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "format_float result should be string");
	ASSERT_STR_EQ("3.14", qd_string_data(elem.value.s), "format_float(3.14)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatFloatNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -2.5);
	usr_strconv_format_float(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("-2.5", qd_string_data(elem.value.s), "format_float(-2.5)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatFloatZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_strconv_format_float(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("0", qd_string_data(elem.value.s), "format_float(0.0)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatFloatWholeNumber) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 100.0);
	usr_strconv_format_float(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("100", qd_string_data(elem.value.s), "format_float(100.0)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatFloatSmall) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.001);
	usr_strconv_format_float(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("0.001", qd_string_data(elem.value.s), "format_float(0.001)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatFloatLarge) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1e10);
	usr_strconv_format_float(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("1e+10", qd_string_data(elem.value.s), "format_float(1e10)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ===== parse_float tests ===== */

TEST(StrconvParseFloatPositive) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "3.14");
	usr_strconv_parse_float(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_float status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_FLOAT, elem.type, "parse_float result should be float");
	ASSERT(elem.value.f > 3.13 && elem.value.f < 3.15, "parse_float(\"3.14\") value");

	destroy_test_context(ctx);
}

TEST(StrconvParseFloatNegative) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "-2.5");
	usr_strconv_parse_float(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_float negative status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT(elem.value.f > -2.51 && elem.value.f < -2.49, "parse_float(\"-2.5\") value");

	destroy_test_context(ctx);
}

TEST(StrconvParseFloatZero) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "0.0");
	usr_strconv_parse_float(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_float zero status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT(elem.value.f == 0.0, "parse_float(\"0.0\") should be 0.0");

	destroy_test_context(ctx);
}

TEST(StrconvParseFloatScientific) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "1.5e2");
	usr_strconv_parse_float(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_float scientific status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT(elem.value.f > 149.9 && elem.value.f < 150.1, "parse_float(\"1.5e2\") should be 150");

	destroy_test_context(ctx);
}

TEST(StrconvParseFloatInvalidError) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "not_a_number");
	int rc = usr_strconv_parse_float(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(2, (int)status.value.i, "parse_float invalid should return error status");
	ASSERT_NE(0, rc, "parse_float invalid should return non-zero rc");

	destroy_test_context(ctx);
}

TEST(StrconvParseFloatEmptyError) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	int rc = usr_strconv_parse_float(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(2, (int)status.value.i, "parse_float empty should return error status");
	ASSERT_NE(0, rc, "parse_float empty should return non-zero rc");

	destroy_test_context(ctx);
}

TEST(StrconvParseFloatWholeNumber) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "42");
	usr_strconv_parse_float(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_float whole number status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT(elem.value.f > 41.9 && elem.value.f < 42.1, "parse_float(\"42\") should be 42.0");

	destroy_test_context(ctx);
}


/* ===== format_bool tests ===== */

TEST(StrconvFormatBoolTrue) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 1);
	usr_strconv_format_bool(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_STR, elem.type, "format_bool result should be string");
	ASSERT_STR_EQ("true", qd_string_data(elem.value.s), "format_bool(1)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatBoolFalse) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	usr_strconv_format_bool(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("false", qd_string_data(elem.value.s), "format_bool(0)");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatBoolNonZeroIsTrue) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	usr_strconv_format_bool(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("true", qd_string_data(elem.value.s), "format_bool(42) non-zero is true");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}

TEST(StrconvFormatBoolNegativeIsTrue) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, -1);
	usr_strconv_format_bool(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_STR_EQ("true", qd_string_data(elem.value.s), "format_bool(-1) negative is true");

	qd_string_release(elem.value.s);
	destroy_test_context(ctx);
}


/* ===== parse_bool tests ===== */

TEST(StrconvParseBoolTrue) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "true");
	usr_strconv_parse_bool(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_bool(\"true\") status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_INT, elem.type, "parse_bool result should be int");
	ASSERT_EQ(1, (int)elem.value.i, "parse_bool(\"true\") should be 1");

	destroy_test_context(ctx);
}

TEST(StrconvParseBoolFalse) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "false");
	usr_strconv_parse_bool(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_bool(\"false\") status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "parse_bool(\"false\") should be 0");

	destroy_test_context(ctx);
}

TEST(StrconvParseBoolOne) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "1");
	usr_strconv_parse_bool(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_bool(\"1\") status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "parse_bool(\"1\") should be 1");

	destroy_test_context(ctx);
}

TEST(StrconvParseBoolZero) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "0");
	usr_strconv_parse_bool(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_bool(\"0\") status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "parse_bool(\"0\") should be 0");

	destroy_test_context(ctx);
}

TEST(StrconvParseBoolCaseInsensitive) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "TRUE");
	usr_strconv_parse_bool(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_bool(\"TRUE\") status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "parse_bool(\"TRUE\") case insensitive");

	destroy_test_context(ctx);
}

TEST(StrconvParseBoolMixedCase) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "False");
	usr_strconv_parse_bool(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "parse_bool(\"False\") status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "parse_bool(\"False\") mixed case");

	destroy_test_context(ctx);
}

TEST(StrconvParseBoolInvalidError) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "yes");
	int rc = usr_strconv_parse_bool(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(2, (int)status.value.i, "parse_bool(\"yes\") should return error status");
	ASSERT_NE(0, rc, "parse_bool(\"yes\") should return non-zero rc");

	destroy_test_context(ctx);
}

TEST(StrconvParseBoolEmptyError) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	int rc = usr_strconv_parse_bool(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(2, (int)status.value.i, "parse_bool(\"\") should return error status");
	ASSERT_NE(0, rc, "parse_bool(\"\") should return non-zero rc");

	destroy_test_context(ctx);
}


/* ===== format_float / parse_float roundtrip ===== */

TEST(StrconvRoundtripFloat) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 2.718);
	usr_strconv_format_float(ctx);
	usr_strconv_parse_float(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "roundtrip float status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(QD_STACK_TYPE_FLOAT, elem.type, "roundtrip float type");
	ASSERT(elem.value.f > 2.717 && elem.value.f < 2.719, "roundtrip float value");

	destroy_test_context(ctx);
}


/* ===== format_bool / parse_bool roundtrip ===== */

TEST(StrconvRoundtripBoolTrue) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 1);
	usr_strconv_format_bool(ctx);
	usr_strconv_parse_bool(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "roundtrip bool true status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(1, (int)elem.value.i, "roundtrip bool true value");

	destroy_test_context(ctx);
}

TEST(StrconvRoundtripBoolFalse) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	usr_strconv_format_bool(ctx);
	usr_strconv_parse_bool(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "roundtrip bool false status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(0, (int)elem.value.i, "roundtrip bool false value");

	destroy_test_context(ctx);
}


/* ===== roundtrip binary ===== */

TEST(StrconvRoundtripBinary) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 255);
	qd_push_i(ctx, 2);
	usr_strconv_format_int(ctx);

	qd_push_i(ctx, 2);
	usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "roundtrip binary status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(255, (int)elem.value.i, "roundtrip binary");

	destroy_test_context(ctx);
}

TEST(StrconvRoundtripOctal) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 511);
	qd_push_i(ctx, 8);
	usr_strconv_format_int(ctx);

	qd_push_i(ctx, 8);
	usr_strconv_parse_int(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "roundtrip octal status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(511, (int)elem.value.i, "roundtrip octal");

	destroy_test_context(ctx);
}

TEST(StrconvRoundtripNegativeDecimal) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, -9999);
	usr_strconv_itoa(ctx);
	usr_strconv_atoi(ctx);

	qd_stack_element_t status;
	qd_stack_pop(ctx->st, &status);
	ASSERT_EQ(1, (int)status.value.i, "roundtrip negative decimal status Ok");

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ(-9999, (int)elem.value.i, "roundtrip negative decimal");

	destroy_test_context(ctx);
}


int main(void) {
	return UC_PrintResults();
}
