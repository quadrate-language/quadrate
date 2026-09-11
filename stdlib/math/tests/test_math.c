/**
 * @file test_math.c
 * @brief Unit tests for the qdmath mathematical functions
 */

#include <quadrate/math/math.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/stack.h>
#include <unit-check/uc.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_E
#define M_E 2.71828182845904523536
#endif

#define EPSILON 1e-9

static qd_context* create_test_context(void) {
	return qd_create_context(256);
}

static void destroy_test_context(qd_context* ctx) {
	qd_free_context(ctx);
}

/* ========================================================================
 * Helper to pop a float result from the stack
 * ======================================================================== */

static double pop_float(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	return elem.value.f;
}

/* ========================================================================
 * Trigonometric Functions
 * ======================================================================== */

TEST(SinZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_sin(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "sin(0) should be 0");

	destroy_test_context(ctx);
}

TEST(SinPiOver2) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, M_PI / 2.0);
	usr_math_sin(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 1.0) < EPSILON, "sin(pi/2) should be 1");

	destroy_test_context(ctx);
}

TEST(SinPi) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, M_PI);
	usr_math_sin(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "sin(pi) should be 0");

	destroy_test_context(ctx);
}

TEST(SinNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -M_PI / 6.0);
	usr_math_sin(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-0.5)) < EPSILON, "sin(-pi/6) should be -0.5");

	destroy_test_context(ctx);
}

TEST(CosZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_cos(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 1.0) < EPSILON, "cos(0) should be 1");

	destroy_test_context(ctx);
}

TEST(CosPiOver2) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, M_PI / 2.0);
	usr_math_cos(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "cos(pi/2) should be 0");

	destroy_test_context(ctx);
}

TEST(CosPi) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, M_PI);
	usr_math_cos(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-1.0)) < EPSILON, "cos(pi) should be -1");

	destroy_test_context(ctx);
}

TEST(TanZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_tan(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "tan(0) should be 0");

	destroy_test_context(ctx);
}

TEST(TanPiOver4) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, M_PI / 4.0);
	usr_math_tan(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 1.0) < EPSILON, "tan(pi/4) should be 1");

	destroy_test_context(ctx);
}

TEST(AsinZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_asin(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "asin(0) should be 0");

	destroy_test_context(ctx);
}

TEST(AsinOne) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_asin(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - M_PI / 2.0) < EPSILON, "asin(1) should be pi/2");

	destroy_test_context(ctx);
}

TEST(AsinHalf) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.5);
	usr_math_asin(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - M_PI / 6.0) < EPSILON, "asin(0.5) should be pi/6");

	destroy_test_context(ctx);
}

TEST(AcosZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_acos(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - M_PI / 2.0) < EPSILON, "acos(0) should be pi/2");

	destroy_test_context(ctx);
}

TEST(AcosOne) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_acos(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "acos(1) should be 0");

	destroy_test_context(ctx);
}

TEST(AtanZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_atan(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "atan(0) should be 0");

	destroy_test_context(ctx);
}

TEST(AtanOne) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_atan(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - M_PI / 4.0) < EPSILON, "atan(1) should be pi/4");

	destroy_test_context(ctx);
}

TEST(Atan2BasicQuadrant) {
	qd_context* ctx = create_test_context();

	/* Stack: push y first, then x (y is deeper, x on top) */
	qd_push_f(ctx, 1.0);  /* y */
	qd_push_f(ctx, 1.0);  /* x */
	usr_math_atan2(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - M_PI / 4.0) < EPSILON, "atan2(1,1) should be pi/4");

	destroy_test_context(ctx);
}

TEST(Atan2YAxis) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);  /* y */
	qd_push_f(ctx, 0.0);  /* x */
	usr_math_atan2(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - M_PI / 2.0) < EPSILON, "atan2(1,0) should be pi/2");

	destroy_test_context(ctx);
}

TEST(Atan2NegativeY) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -1.0);  /* y */
	qd_push_f(ctx, 0.0);   /* x */
	usr_math_atan2(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-M_PI / 2.0)) < EPSILON, "atan2(-1,0) should be -pi/2");

	destroy_test_context(ctx);
}

/* ========================================================================
 * Hyperbolic Functions
 * ======================================================================== */

TEST(SinhZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_sinh(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "sinh(0) should be 0");

	destroy_test_context(ctx);
}

TEST(SinhOne) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_sinh(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - sinh(1.0)) < EPSILON, "sinh(1) should match libc sinh(1)");

	destroy_test_context(ctx);
}

TEST(CoshZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_cosh(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 1.0) < EPSILON, "cosh(0) should be 1");

	destroy_test_context(ctx);
}

TEST(CoshOne) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_cosh(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - cosh(1.0)) < EPSILON, "cosh(1) should match libc cosh(1)");

	destroy_test_context(ctx);
}

TEST(TanhZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_tanh(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "tanh(0) should be 0");

	destroy_test_context(ctx);
}

TEST(TanhOne) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_tanh(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - tanh(1.0)) < EPSILON, "tanh(1) should match libc tanh(1)");

	destroy_test_context(ctx);
}

TEST(AsinhZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_asinh(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "asinh(0) should be 0");

	destroy_test_context(ctx);
}

TEST(AsinhOne) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_asinh(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - asinh(1.0)) < EPSILON, "asinh(1) should match libc asinh(1)");

	destroy_test_context(ctx);
}

TEST(AcoshOne) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_acosh(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "acosh(1) should be 0");

	destroy_test_context(ctx);
}

TEST(AcoshTwo) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 2.0);
	usr_math_acosh(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - acosh(2.0)) < EPSILON, "acosh(2) should match libc acosh(2)");

	destroy_test_context(ctx);
}

TEST(AtanhZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_atanh(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "atanh(0) should be 0");

	destroy_test_context(ctx);
}

TEST(AtanhHalf) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.5);
	usr_math_atanh(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - atanh(0.5)) < EPSILON, "atanh(0.5) should match libc atanh(0.5)");

	destroy_test_context(ctx);
}

/* ========================================================================
 * Power and Root Functions
 * ======================================================================== */

TEST(SqrtFour) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 4.0);
	usr_math_sqrt(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 2.0) < EPSILON, "sqrt(4) should be 2");

	destroy_test_context(ctx);
}

TEST(SqrtZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_sqrt(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "sqrt(0) should be 0");

	destroy_test_context(ctx);
}

TEST(SqrtTwo) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 2.0);
	usr_math_sqrt(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - sqrt(2.0)) < EPSILON, "sqrt(2) should match libc sqrt(2)");

	destroy_test_context(ctx);
}

TEST(SqThree) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.0);
	usr_math_sq(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 9.0) < EPSILON, "sq(3) should be 9");

	destroy_test_context(ctx);
}

TEST(SqNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -4.0);
	usr_math_sq(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 16.0) < EPSILON, "sq(-4) should be 16");

	destroy_test_context(ctx);
}

TEST(SqZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_sq(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "sq(0) should be 0");

	destroy_test_context(ctx);
}

TEST(CbTwo) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 2.0);
	usr_math_cb(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 8.0) < EPSILON, "cb(2) should be 8");

	destroy_test_context(ctx);
}

TEST(CbNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -3.0);
	usr_math_cb(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-27.0)) < EPSILON, "cb(-3) should be -27");

	destroy_test_context(ctx);
}

TEST(CbrtEight) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 8.0);
	usr_math_cbrt(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 2.0) < EPSILON, "cbrt(8) should be 2");

	destroy_test_context(ctx);
}

TEST(CbrtTwentySeven) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 27.0);
	usr_math_cbrt(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 3.0) < EPSILON, "cbrt(27) should be 3");

	destroy_test_context(ctx);
}

TEST(CbrtNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -8.0);
	usr_math_cbrt(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-2.0)) < EPSILON, "cbrt(-8) should be -2");

	destroy_test_context(ctx);
}

TEST(PowSquare) {
	qd_context* ctx = create_test_context();

	/* Stack: push base first, then exponent */
	qd_push_f(ctx, 2.0);  /* base */
	qd_push_f(ctx, 10.0); /* exponent */
	usr_math_pow(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 1024.0) < EPSILON, "pow(2,10) should be 1024");

	destroy_test_context(ctx);
}

TEST(PowFractional) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 9.0);  /* base */
	qd_push_f(ctx, 0.5);  /* exponent */
	usr_math_pow(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 3.0) < EPSILON, "pow(9, 0.5) should be 3");

	destroy_test_context(ctx);
}

TEST(PowZeroExponent) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 5.0);  /* base */
	qd_push_f(ctx, 0.0);  /* exponent */
	usr_math_pow(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 1.0) < EPSILON, "pow(5, 0) should be 1");

	destroy_test_context(ctx);
}

TEST(ExpZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_exp(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 1.0) < EPSILON, "exp(0) should be 1");

	destroy_test_context(ctx);
}

TEST(ExpOne) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_exp(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - M_E) < EPSILON, "exp(1) should be e");

	destroy_test_context(ctx);
}

TEST(ExpNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -1.0);
	usr_math_exp(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (1.0 / M_E)) < EPSILON, "exp(-1) should be 1/e");

	destroy_test_context(ctx);
}

TEST(HypotThreeFour) {
	qd_context* ctx = create_test_context();

	/* Stack: push x first, then y */
	qd_push_f(ctx, 3.0);  /* x */
	qd_push_f(ctx, 4.0);  /* y */
	usr_math_hypot(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 5.0) < EPSILON, "hypot(3,4) should be 5");

	destroy_test_context(ctx);
}

TEST(HypotZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	qd_push_f(ctx, 0.0);
	usr_math_hypot(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "hypot(0,0) should be 0");

	destroy_test_context(ctx);
}

TEST(HypotFiveTwelve) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 5.0);
	qd_push_f(ctx, 12.0);
	usr_math_hypot(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 13.0) < EPSILON, "hypot(5,12) should be 13");

	destroy_test_context(ctx);
}

/* ========================================================================
 * Logarithmic Functions
 * ======================================================================== */

TEST(LnOne) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_ln(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "ln(1) should be 0");

	destroy_test_context(ctx);
}

TEST(LnE) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, M_E);
	usr_math_ln(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 1.0) < EPSILON, "ln(e) should be 1");

	destroy_test_context(ctx);
}

TEST(LnESquared) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, M_E * M_E);
	usr_math_ln(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 2.0) < EPSILON, "ln(e^2) should be 2");

	destroy_test_context(ctx);
}

TEST(Log10One) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_log10(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "log10(1) should be 0");

	destroy_test_context(ctx);
}

TEST(Log10Hundred) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 100.0);
	usr_math_log10(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 2.0) < EPSILON, "log10(100) should be 2");

	destroy_test_context(ctx);
}

TEST(Log10Thousand) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1000.0);
	usr_math_log10(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 3.0) < EPSILON, "log10(1000) should be 3");

	destroy_test_context(ctx);
}

TEST(Log2One) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_log2(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "log2(1) should be 0");

	destroy_test_context(ctx);
}

TEST(Log2Eight) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 8.0);
	usr_math_log2(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 3.0) < EPSILON, "log2(8) should be 3");

	destroy_test_context(ctx);
}

TEST(Log2OneHalf) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.5);
	usr_math_log2(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-1.0)) < EPSILON, "log2(0.5) should be -1");

	destroy_test_context(ctx);
}

TEST(Exp2Zero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_exp2(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 1.0) < EPSILON, "exp2(0) should be 1");

	destroy_test_context(ctx);
}

TEST(Exp2Three) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.0);
	usr_math_exp2(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 8.0) < EPSILON, "exp2(3) should be 8");

	destroy_test_context(ctx);
}

TEST(Exp2Negative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -1.0);
	usr_math_exp2(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.5) < EPSILON, "exp2(-1) should be 0.5");

	destroy_test_context(ctx);
}

/* ========================================================================
 * Rounding Functions
 * ======================================================================== */

TEST(CeilPositive) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 2.3);
	usr_math_ceil(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 3.0) < EPSILON, "ceil(2.3) should be 3");

	destroy_test_context(ctx);
}

TEST(CeilNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -2.3);
	usr_math_ceil(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-2.0)) < EPSILON, "ceil(-2.3) should be -2");

	destroy_test_context(ctx);
}

TEST(CeilWhole) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 5.0);
	usr_math_ceil(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 5.0) < EPSILON, "ceil(5.0) should be 5");

	destroy_test_context(ctx);
}

TEST(FloorPositive) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 2.7);
	usr_math_floor(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 2.0) < EPSILON, "floor(2.7) should be 2");

	destroy_test_context(ctx);
}

TEST(FloorNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -2.3);
	usr_math_floor(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-3.0)) < EPSILON, "floor(-2.3) should be -3");

	destroy_test_context(ctx);
}

TEST(FloorWhole) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 5.0);
	usr_math_floor(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 5.0) < EPSILON, "floor(5.0) should be 5");

	destroy_test_context(ctx);
}

TEST(RoundUp) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 2.6);
	usr_math_round(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 3.0) < EPSILON, "round(2.6) should be 3");

	destroy_test_context(ctx);
}

TEST(RoundDown) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 2.3);
	usr_math_round(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 2.0) < EPSILON, "round(2.3) should be 2");

	destroy_test_context(ctx);
}

TEST(RoundHalf) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 2.5);
	usr_math_round(ctx);

	double result = pop_float(ctx);
	/* C round() rounds 0.5 away from zero */
	ASSERT(fabs(result - 3.0) < EPSILON, "round(2.5) should be 3");

	destroy_test_context(ctx);
}

TEST(RoundNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -2.5);
	usr_math_round(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-3.0)) < EPSILON, "round(-2.5) should be -3");

	destroy_test_context(ctx);
}

TEST(TruncPositive) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 2.9);
	usr_math_trunc(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 2.0) < EPSILON, "trunc(2.9) should be 2");

	destroy_test_context(ctx);
}

TEST(TruncNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -2.9);
	usr_math_trunc(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-2.0)) < EPSILON, "trunc(-2.9) should be -2");

	destroy_test_context(ctx);
}

TEST(FmodBasic) {
	qd_context* ctx = create_test_context();

	/* Stack: push x first, then y -- computes x mod y */
	qd_push_f(ctx, 10.0);  /* x */
	qd_push_f(ctx, 3.0);   /* y */
	usr_math_fmod(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 1.0) < EPSILON, "fmod(10, 3) should be 1");

	destroy_test_context(ctx);
}

TEST(FmodFractional) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 5.5);  /* x */
	qd_push_f(ctx, 2.2);  /* y */
	usr_math_fmod(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - fmod(5.5, 2.2)) < EPSILON, "fmod(5.5, 2.2) should match libc fmod");

	destroy_test_context(ctx);
}

TEST(FmodNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -10.0);  /* x */
	qd_push_f(ctx, 3.0);    /* y */
	usr_math_fmod(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - fmod(-10.0, 3.0)) < EPSILON, "fmod(-10, 3) should match libc fmod");

	destroy_test_context(ctx);
}

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

TEST(AbsPositive) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 5.0);
	usr_math_abs(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 5.0) < EPSILON, "abs(5) should be 5");

	destroy_test_context(ctx);
}

TEST(AbsNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -3.7);
	usr_math_abs(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 3.7) < EPSILON, "abs(-3.7) should be 3.7");

	destroy_test_context(ctx);
}

TEST(AbsZero) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.0);
	usr_math_abs(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.0) < EPSILON, "abs(0) should be 0");

	destroy_test_context(ctx);
}

TEST(MinBasic) {
	qd_context* ctx = create_test_context();

	/* Stack: push a first, then b */
	qd_push_f(ctx, 3.0);  /* a */
	qd_push_f(ctx, 7.0);  /* b */
	usr_math_min(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 3.0) < EPSILON, "min(3, 7) should be 3");

	destroy_test_context(ctx);
}

TEST(MinEqual) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 5.0);
	qd_push_f(ctx, 5.0);
	usr_math_min(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 5.0) < EPSILON, "min(5, 5) should be 5");

	destroy_test_context(ctx);
}

TEST(MinNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -2.0);
	qd_push_f(ctx, -8.0);
	usr_math_min(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-8.0)) < EPSILON, "min(-2, -8) should be -8");

	destroy_test_context(ctx);
}

TEST(MaxBasic) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 3.0);
	qd_push_f(ctx, 7.0);
	usr_math_max(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 7.0) < EPSILON, "max(3, 7) should be 7");

	destroy_test_context(ctx);
}

TEST(MaxEqual) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 5.0);
	qd_push_f(ctx, 5.0);
	usr_math_max(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 5.0) < EPSILON, "max(5, 5) should be 5");

	destroy_test_context(ctx);
}

TEST(MaxNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -2.0);
	qd_push_f(ctx, -8.0);
	usr_math_max(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-2.0)) < EPSILON, "max(-2, -8) should be -2");

	destroy_test_context(ctx);
}

TEST(FacZero) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 0);
	usr_math_fac(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "0! should be 1");

	destroy_test_context(ctx);
}

TEST(FacOne) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 1);
	usr_math_fac(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 1, "1! should be 1");

	destroy_test_context(ctx);
}

TEST(FacFive) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 5);
	usr_math_fac(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 120, "5! should be 120");

	destroy_test_context(ctx);
}

TEST(FacTen) {
	qd_context* ctx = create_test_context();

	qd_push_i(ctx, 10);
	usr_math_fac(ctx);

	qd_stack_element_t elem;
	qd_stack_pop(ctx->st, &elem);
	ASSERT_EQ((int)elem.value.i, 3628800, "10! should be 3628800");

	destroy_test_context(ctx);
}

TEST(InvTwo) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 2.0);
	usr_math_inv(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.5) < EPSILON, "inv(2) should be 0.5");

	destroy_test_context(ctx);
}

TEST(InvFour) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 4.0);
	usr_math_inv(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.25) < EPSILON, "inv(4) should be 0.25");

	destroy_test_context(ctx);
}

TEST(InvNegative) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, -5.0);
	usr_math_inv(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - (-0.2)) < EPSILON, "inv(-5) should be -0.2");

	destroy_test_context(ctx);
}

TEST(InvOne) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 1.0);
	usr_math_inv(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 1.0) < EPSILON, "inv(1) should be 1");

	destroy_test_context(ctx);
}

/* ========================================================================
 * Cross-checks: verify inverse relationships
 * ======================================================================== */

TEST(SinAsinRoundTrip) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.7);
	usr_math_sin(ctx);
	usr_math_asin(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.7) < EPSILON, "asin(sin(0.7)) should be 0.7");

	destroy_test_context(ctx);
}

TEST(CosAcosRoundTrip) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 0.7);
	usr_math_cos(ctx);
	usr_math_acos(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 0.7) < EPSILON, "acos(cos(0.7)) should be 0.7");

	destroy_test_context(ctx);
}

TEST(ExpLnRoundTrip) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 2.5);
	usr_math_exp(ctx);
	usr_math_ln(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 2.5) < EPSILON, "ln(exp(2.5)) should be 2.5");

	destroy_test_context(ctx);
}

TEST(SqSqrtRoundTrip) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 7.0);
	usr_math_sq(ctx);
	usr_math_sqrt(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 7.0) < EPSILON, "sqrt(sq(7)) should be 7");

	destroy_test_context(ctx);
}

TEST(CbCbrtRoundTrip) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 5.0);
	usr_math_cb(ctx);
	usr_math_cbrt(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 5.0) < EPSILON, "cbrt(cb(5)) should be 5");

	destroy_test_context(ctx);
}

TEST(Exp2Log2RoundTrip) {
	qd_context* ctx = create_test_context();

	qd_push_f(ctx, 4.5);
	usr_math_exp2(ctx);
	usr_math_log2(ctx);

	double result = pop_float(ctx);
	ASSERT(fabs(result - 4.5) < EPSILON, "log2(exp2(4.5)) should be 4.5");

	destroy_test_context(ctx);
}

int main(void) {
	return UC_PrintResults();
}
