/**
 * @file test_testing.c
 * @brief Unit tests for the qdtesting assertion library
 *
 * Tests each assertion function in both pass and fail scenarios.
 * The assertion functions return 0 on success and non-zero on failure,
 * so we verify return codes rather than trapping aborts.
 */

#define _POSIX_C_SOURCE 200809L

#include <quadrate/testing/testing.h>
#include <quadrate/rt/runtime.h>
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

/* ===================================================================
 * assert_eq - pass cases
 * =================================================================== */

TEST(AssertEqIntPass) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_push_i(ctx, 42);
	int rc = usr_testing_assert_eq(ctx);
	ASSERT_EQ(0, rc, "assert_eq should pass for equal ints");
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after assert_eq");

	destroy_test_context(ctx);
}

TEST(AssertEqFloatPass) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.14);
	qd_push_f(ctx, 3.14);
	int rc = usr_testing_assert_eq(ctx);
	ASSERT_EQ(0, rc, "assert_eq should pass for equal floats");

	destroy_test_context(ctx);
}

TEST(AssertEqStrPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "hello");
	int rc = usr_testing_assert_eq(ctx);
	ASSERT_EQ(0, rc, "assert_eq should pass for equal strings");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_eq - fail cases
 * =================================================================== */

TEST(AssertEqIntFail) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_push_i(ctx, 99);
	int rc = usr_testing_assert_eq(ctx);
	ASSERT_NE(0, rc, "assert_eq should fail for different ints");
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after failed assert_eq");

	destroy_test_context(ctx);
}

TEST(AssertEqStrFail) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "world");
	int rc = usr_testing_assert_eq(ctx);
	ASSERT_NE(0, rc, "assert_eq should fail for different strings");

	destroy_test_context(ctx);
}

TEST(AssertEqTypeMismatchFail) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_push_f(ctx, 42.0);
	int rc = usr_testing_assert_eq(ctx);
	ASSERT_NE(0, rc, "assert_eq should fail for different types");

	destroy_test_context(ctx);
}

TEST(AssertEqInsufficientStack) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	int rc = usr_testing_assert_eq(ctx);
	ASSERT_NE(0, rc, "assert_eq should fail with only 1 value on stack");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_ne - pass cases
 * =================================================================== */

TEST(AssertNeIntPass) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_push_i(ctx, 99);
	int rc = usr_testing_assert_ne(ctx);
	ASSERT_EQ(0, rc, "assert_ne should pass for different ints");
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after assert_ne");

	destroy_test_context(ctx);
}

TEST(AssertNeStrPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "world");
	int rc = usr_testing_assert_ne(ctx);
	ASSERT_EQ(0, rc, "assert_ne should pass for different strings");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_ne - fail cases
 * =================================================================== */

TEST(AssertNeIntFail) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_push_i(ctx, 42);
	int rc = usr_testing_assert_ne(ctx);
	ASSERT_NE(0, rc, "assert_ne should fail for equal ints");

	destroy_test_context(ctx);
}

TEST(AssertNeStrFail) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "hello");
	int rc = usr_testing_assert_ne(ctx);
	ASSERT_NE(0, rc, "assert_ne should fail for equal strings");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_true - pass cases
 * =================================================================== */

TEST(AssertTrueIntPass) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 1);
	int rc = usr_testing_assert_true(ctx);
	ASSERT_EQ(0, rc, "assert_true should pass for non-zero int");
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after assert_true");

	destroy_test_context(ctx);
}

TEST(AssertTrueNegativeIntPass) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, -1);
	int rc = usr_testing_assert_true(ctx);
	ASSERT_EQ(0, rc, "assert_true should pass for negative int");

	destroy_test_context(ctx);
}

TEST(AssertTrueFloatPass) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	int rc = usr_testing_assert_true(ctx);
	ASSERT_EQ(0, rc, "assert_true should pass for non-zero float");

	destroy_test_context(ctx);
}

TEST(AssertTrueStrPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	int rc = usr_testing_assert_true(ctx);
	ASSERT_EQ(0, rc, "assert_true should pass for non-empty string");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_true - fail cases
 * =================================================================== */

TEST(AssertTrueIntFail) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	int rc = usr_testing_assert_true(ctx);
	ASSERT_NE(0, rc, "assert_true should fail for zero");

	destroy_test_context(ctx);
}

TEST(AssertTrueFloatFail) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	int rc = usr_testing_assert_true(ctx);
	ASSERT_NE(0, rc, "assert_true should fail for 0.0");

	destroy_test_context(ctx);
}

TEST(AssertTrueEmptyStrFail) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	int rc = usr_testing_assert_true(ctx);
	ASSERT_NE(0, rc, "assert_true should fail for empty string");

	destroy_test_context(ctx);
}

TEST(AssertTrueEmptyStack) {
	qd_context* ctx = create_test_context();

	int rc = usr_testing_assert_true(ctx);
	ASSERT_NE(0, rc, "assert_true should fail with empty stack");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_false - pass cases
 * =================================================================== */

TEST(AssertFalseIntPass) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	int rc = usr_testing_assert_false(ctx);
	ASSERT_EQ(0, rc, "assert_false should pass for zero");
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after assert_false");

	destroy_test_context(ctx);
}

TEST(AssertFalseFloatPass) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	int rc = usr_testing_assert_false(ctx);
	ASSERT_EQ(0, rc, "assert_false should pass for 0.0");

	destroy_test_context(ctx);
}

TEST(AssertFalseEmptyStrPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "");
	int rc = usr_testing_assert_false(ctx);
	ASSERT_EQ(0, rc, "assert_false should pass for empty string");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_false - fail cases
 * =================================================================== */

TEST(AssertFalseIntFail) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 1);
	int rc = usr_testing_assert_false(ctx);
	ASSERT_NE(0, rc, "assert_false should fail for non-zero int");

	destroy_test_context(ctx);
}

TEST(AssertFalseStrFail) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	int rc = usr_testing_assert_false(ctx);
	ASSERT_NE(0, rc, "assert_false should fail for non-empty string");

	destroy_test_context(ctx);
}

/* ===================================================================
 * fail
 * =================================================================== */

TEST(FailAlwaysFails) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "intentional failure");
	int rc = usr_testing_fail(ctx);
	ASSERT_NE(0, rc, "fail should always return non-zero");
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after fail");

	destroy_test_context(ctx);
}

TEST(FailEmptyStack) {
	qd_context* ctx = create_test_context();

	int rc = usr_testing_fail(ctx);
	ASSERT_NE(0, rc, "fail should return non-zero even with empty stack");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_approx_eq - pass cases
 * =================================================================== */

TEST(AssertApproxEqPass) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.14159);
	qd_push_f(ctx, 3.14160);
	qd_push_f(ctx, 0.001);
	int rc = usr_testing_assert_approx_eq(ctx);
	ASSERT_EQ(0, rc, "assert_approx_eq should pass within epsilon");
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after assert_approx_eq");

	destroy_test_context(ctx);
}

TEST(AssertApproxEqExactPass) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	qd_push_f(ctx, 1.0);
	qd_push_f(ctx, 0.0);
	int rc = usr_testing_assert_approx_eq(ctx);
	ASSERT_EQ(0, rc, "assert_approx_eq should pass for exactly equal values");

	destroy_test_context(ctx);
}

TEST(AssertApproxEqBoundaryPass) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	qd_push_f(ctx, 1.5);
	qd_push_f(ctx, 0.5);
	int rc = usr_testing_assert_approx_eq(ctx);
	ASSERT_EQ(0, rc, "assert_approx_eq should pass when diff equals epsilon");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_approx_eq - fail cases
 * =================================================================== */

TEST(AssertApproxEqFail) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	qd_push_f(ctx, 2.0);
	qd_push_f(ctx, 0.5);
	int rc = usr_testing_assert_approx_eq(ctx);
	ASSERT_NE(0, rc, "assert_approx_eq should fail when diff exceeds epsilon");

	destroy_test_context(ctx);
}

TEST(AssertApproxEqWrongType) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 1);
	qd_push_f(ctx, 1.0);
	qd_push_f(ctx, 0.1);
	int rc = usr_testing_assert_approx_eq(ctx);
	ASSERT_NE(0, rc, "assert_approx_eq should fail with non-float argument");

	destroy_test_context(ctx);
}

TEST(AssertApproxEqInsufficientStack) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	qd_push_f(ctx, 1.0);
	int rc = usr_testing_assert_approx_eq(ctx);
	ASSERT_NE(0, rc, "assert_approx_eq should fail with only 2 values on stack");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_contains - pass cases
 * =================================================================== */

TEST(AssertContainsPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "world");
	int rc = usr_testing_assert_contains(ctx);
	ASSERT_EQ(0, rc, "assert_contains should pass when substring is present");
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after assert_contains");

	destroy_test_context(ctx);
}

TEST(AssertContainsAtStartPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "hello");
	int rc = usr_testing_assert_contains(ctx);
	ASSERT_EQ(0, rc, "assert_contains should pass for prefix substring");

	destroy_test_context(ctx);
}

TEST(AssertContainsEmptyNeedlePass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "");
	int rc = usr_testing_assert_contains(ctx);
	ASSERT_EQ(0, rc, "assert_contains should pass for empty needle");

	destroy_test_context(ctx);
}

TEST(AssertContainsExactMatchPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "hello");
	int rc = usr_testing_assert_contains(ctx);
	ASSERT_EQ(0, rc, "assert_contains should pass for exact match");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_contains - fail cases
 * =================================================================== */

TEST(AssertContainsFail) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "xyz");
	int rc = usr_testing_assert_contains(ctx);
	ASSERT_NE(0, rc, "assert_contains should fail when substring is absent");

	destroy_test_context(ctx);
}

TEST(AssertContainsNonStringFail) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 42);
	qd_push_s(ctx, "42");
	int rc = usr_testing_assert_contains(ctx);
	ASSERT_NE(0, rc, "assert_contains should fail with non-string haystack");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_starts_with - pass cases
 * =================================================================== */

TEST(AssertStartsWithPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "hello");
	int rc = usr_testing_assert_starts_with(ctx);
	ASSERT_EQ(0, rc, "assert_starts_with should pass for correct prefix");
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after assert_starts_with");

	destroy_test_context(ctx);
}

TEST(AssertStartsWithEmptyPrefixPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "");
	int rc = usr_testing_assert_starts_with(ctx);
	ASSERT_EQ(0, rc, "assert_starts_with should pass for empty prefix");

	destroy_test_context(ctx);
}

TEST(AssertStartsWithFullMatchPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "hello");
	int rc = usr_testing_assert_starts_with(ctx);
	ASSERT_EQ(0, rc, "assert_starts_with should pass for full match");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_starts_with - fail cases
 * =================================================================== */

TEST(AssertStartsWithFail) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "world");
	int rc = usr_testing_assert_starts_with(ctx);
	ASSERT_NE(0, rc, "assert_starts_with should fail for wrong prefix");

	destroy_test_context(ctx);
}

TEST(AssertStartsWithLongerPrefixFail) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hi");
	qd_push_s(ctx, "hello");
	int rc = usr_testing_assert_starts_with(ctx);
	ASSERT_NE(0, rc, "assert_starts_with should fail when prefix is longer than string");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_ends_with - pass cases
 * =================================================================== */

TEST(AssertEndsWithPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "world");
	int rc = usr_testing_assert_ends_with(ctx);
	ASSERT_EQ(0, rc, "assert_ends_with should pass for correct suffix");
	ASSERT_EQ(0, (int)qd_stack_size(ctx->st), "stack should be empty after assert_ends_with");

	destroy_test_context(ctx);
}

TEST(AssertEndsWithEmptySuffixPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "");
	int rc = usr_testing_assert_ends_with(ctx);
	ASSERT_EQ(0, rc, "assert_ends_with should pass for empty suffix");

	destroy_test_context(ctx);
}

TEST(AssertEndsWithFullMatchPass) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello");
	qd_push_s(ctx, "hello");
	int rc = usr_testing_assert_ends_with(ctx);
	ASSERT_EQ(0, rc, "assert_ends_with should pass for full match");

	destroy_test_context(ctx);
}

/* ===================================================================
 * assert_ends_with - fail cases
 * =================================================================== */

TEST(AssertEndsWithFail) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hello world");
	qd_push_s(ctx, "hello");
	int rc = usr_testing_assert_ends_with(ctx);
	ASSERT_NE(0, rc, "assert_ends_with should fail for wrong suffix");

	destroy_test_context(ctx);
}

TEST(AssertEndsWithLongerSuffixFail) {
	qd_context* ctx = create_test_context();

	qd_push_s(ctx, "hi");
	qd_push_s(ctx, "hello");
	int rc = usr_testing_assert_ends_with(ctx);
	ASSERT_NE(0, rc, "assert_ends_with should fail when suffix is longer than string");

	destroy_test_context(ctx);
}


int main(void) {
	return UC_PrintResults();
}
