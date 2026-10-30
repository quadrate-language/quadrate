// math:: -- the stack words over libm.
//
// Every word takes its operands as f64 -- an i64 widens, as it does in arithmetic -- through
// qd_arg_f, which raises on a missing operand or one of another type. Most are one libm call;
// the few that check their domain first say so beside their definitions.

#include <math.h>
#include <quadrate/math/math.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <stdint.h>

static int push_f64(qd_context* ctx, const char* op, double value) {
	if (qd_stack_push_float(ctx->st, value) != QD_STACK_OK) {
		qd_fatal_raise(ctx, op, "Stack overflow");
	}
	return 0;
}

static int push_i64(qd_context* ctx, const char* op, int64_t value) {
	if (qd_stack_push_int(ctx->st, value) != QD_STACK_OK) {
		qd_fatal_raise(ctx, op, "Stack overflow");
	}
	return 0;
}

// ( x -- fn(x) )
#define MATH_UNARY(word, fn)                                                                                           \
	int usr_math_##word(qd_context* ctx) {                                                                             \
		return push_f64(ctx, "math::" #word, fn(qd_arg_f(ctx, "math::" #word)));                                       \
	}

// ( a b -- fn(a, b) ): b is on top, so it comes off first
#define MATH_BINARY(word, fn)                                                                                          \
	int usr_math_##word(qd_context* ctx) {                                                                             \
		const double b = qd_arg_f(ctx, "math::" #word);                                                                \
		const double a = qd_arg_f(ctx, "math::" #word);                                                                \
		return push_f64(ctx, "math::" #word, fn(a, b));                                                                \
	}

MATH_UNARY(sin, sin)
MATH_UNARY(cos, cos)
MATH_UNARY(tan, tan)
MATH_UNARY(atan, atan)
MATH_UNARY(sinh, sinh)
MATH_UNARY(cosh, cosh)
MATH_UNARY(tanh, tanh)
MATH_UNARY(asinh, asinh)
MATH_UNARY(cbrt, cbrt)
MATH_UNARY(exp, exp)
MATH_UNARY(exp2, exp2)
MATH_UNARY(ceil, ceil)
MATH_UNARY(floor, floor)
MATH_UNARY(round, round)
MATH_UNARY(trunc, trunc)

MATH_BINARY(atan2, atan2)
MATH_BINARY(pow, pow)
MATH_BINARY(hypot, hypot)
MATH_BINARY(fmod, fmod)

// The words below reject an operand outside their domain rather than return NaN

int usr_math_asin(qd_context* ctx) {
	const double value = qd_arg_f(ctx, "math::asin");
	if (value < -1.0 || value > 1.0) {
		qd_fatal_raise(ctx, "math::asin", "Domain error (value %f is outside [-1, 1])", value);
	}
	return push_f64(ctx, "math::asin", asin(value));
}

int usr_math_acos(qd_context* ctx) {
	const double value = qd_arg_f(ctx, "math::acos");
	if (value < -1.0 || value > 1.0) {
		qd_fatal_raise(ctx, "math::acos", "Domain error (value %f is outside [-1, 1])", value);
	}
	return push_f64(ctx, "math::acos", acos(value));
}

int usr_math_sqrt(qd_context* ctx) {
	const double value = qd_arg_f(ctx, "math::sqrt");
	if (value < 0.0) {
		qd_fatal_raise(ctx, "math::sqrt", "Domain error (requires non-negative value, got %f)", value);
	}
	return push_f64(ctx, "math::sqrt", sqrt(value));
}

int usr_math_ln(qd_context* ctx) {
	const double value = qd_arg_f(ctx, "math::ln");
	if (value <= 0.0) {
		qd_fatal_raise(ctx, "math::ln", "Domain error (requires positive value, got %f)", value);
	}
	return push_f64(ctx, "math::ln", log(value));
}

int usr_math_log10(qd_context* ctx) {
	const double value = qd_arg_f(ctx, "math::log10");
	if (value <= 0.0) {
		qd_fatal_raise(ctx, "math::log10", "Domain error (requires positive value, got %f)", value);
	}
	return push_f64(ctx, "math::log10", log10(value));
}

int usr_math_log2(qd_context* ctx) {
	const double value = qd_arg_f(ctx, "math::log2");
	if (value <= 0.0) {
		qd_fatal_raise(ctx, "math::log2", "Domain error (requires positive value)");
	}
	return push_f64(ctx, "math::log2", log2(value));
}

int usr_math_acosh(qd_context* ctx) {
	const double value = qd_arg_f(ctx, "math::acosh");
	if (value < 1.0) {
		qd_fatal_raise(ctx, "math::acosh", "Domain error (requires value >= 1)");
	}
	return push_f64(ctx, "math::acosh", acosh(value));
}

int usr_math_atanh(qd_context* ctx) {
	const double value = qd_arg_f(ctx, "math::atanh");
	if (value <= -1.0 || value >= 1.0) {
		qd_fatal_raise(ctx, "math::atanh", "Domain error (requires -1 < value < 1)");
	}
	return push_f64(ctx, "math::atanh", atanh(value));
}

// ( n:i64 -- n!:i64 )
int usr_math_fac(qd_context* ctx) {
	const int64_t n = qd_arg_i(ctx, "math::fac");
	if (n < 0) {
		qd_fatal_raise(ctx, "math::fac", "Domain error (requires non-negative integer)");
	}
	int64_t result = 1;
	for (int64_t i = 2; i <= n; i++) {
		// Detect overflow before it happens (21! already exceeds int64).
		if (result > INT64_MAX / i) {
			qd_fatal_raise(ctx, "math::fac", "Result overflow (n too large)");
		}
		result *= i;
	}
	return push_i64(ctx, "math::fac", result);
}

// inf / nan - name the two IEEE 754 values that have no literal spelling.
// Division produces them (1.0 0.0 / is +infinity, 0.0 0.0 / is NaN), but without
// these a program cannot write one down -- which matters for the usual seeds:
// "smallest so far" starts at +infinity, "no result yet" at NaN.
int usr_math_inf(qd_context* ctx) {
	// HUGE_VAL, not INFINITY: the latter is a float and -Wdouble-promotion rejects widening it.
	return push_f64(ctx, "math::inf", HUGE_VAL);
}

int usr_math_nan(qd_context* ctx) {
	// nan("") is double; the NAN macro is a float.
	return push_f64(ctx, "math::nan", nan(""));
}

// Classification predicates. NaN is not equal to itself, so `x x ==` is the only
// portable NaN test without these, and it reads as a mistake. isnan, isinf and
// isfinite are macros, so each is written out rather than passed as a function.
int usr_math_is_nan(qd_context* ctx) {
	return push_i64(ctx, "math::is_nan", isnan(qd_arg_f(ctx, "math::is_nan")) ? 1 : 0);
}

int usr_math_is_inf(qd_context* ctx) {
	return push_i64(ctx, "math::is_inf", isinf(qd_arg_f(ctx, "math::is_inf")) ? 1 : 0);
}

int usr_math_is_finite(qd_context* ctx) {
	return push_i64(ctx, "math::is_finite", isfinite(qd_arg_f(ctx, "math::is_finite")) ? 1 : 0);
}
