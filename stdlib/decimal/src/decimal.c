// decimal:: -- the stack words over the runtime's decimal128 functions (quadrate/rt/d128.h).
//
// The arithmetic is the runtime's; each word here pops its operands, calls the function of
// the same name and pushes what it returns. The compiler has already checked that every
// operand is a d128, so an operand that is not one is a program that went around it -- an
// `any` parameter -- and is fatal rather than something to recover from.

#include <quadrate/decimal/decimal.h>
#include <quadrate/rt/d128.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <stdint.h>

// An i64 argument widens to a d128 exactly, as it does in arithmetic
static qd_d128_t pop_d128(qd_context* ctx, const char* word) {
	qd_stack_element_t elem;
	if (qd_stack_pop(ctx->st, &elem) != QD_STACK_OK) {
		qd_fatal_raise(ctx, word, "Stack underflow");
	}
	if (elem.type == QD_STACK_TYPE_INT) {
		return qd_d128_from_i64(elem.value.i);
	}
	if (elem.type != QD_STACK_TYPE_D128) {
		qd_fatal_raise(ctx, word, "Type error (expected d128)");
	}
	return elem.value.d;
}

static int64_t pop_i64(qd_context* ctx, const char* word) {
	qd_stack_element_t elem;
	if (qd_stack_pop(ctx->st, &elem) != QD_STACK_OK) {
		qd_fatal_raise(ctx, word, "Stack underflow");
	}
	if (elem.type != QD_STACK_TYPE_INT) {
		qd_fatal_raise(ctx, word, "Type error (expected i64)");
	}
	return elem.value.i;
}

static int push_d128(qd_context* ctx, const char* word, qd_d128_t value) {
	if (qd_stack_push_d128(ctx->st, value) != QD_STACK_OK) {
		qd_fatal_raise(ctx, word, "Stack overflow");
	}
	return 0;
}

static int push_i64(qd_context* ctx, const char* word, int64_t value) {
	if (qd_stack_push_int(ctx->st, value) != QD_STACK_OK) {
		qd_fatal_raise(ctx, word, "Stack overflow");
	}
	return 0;
}

#define D128_UNARY(word)                                                                                               \
	int usr_decimal_##word(qd_context* ctx) {                                                                          \
		const qd_d128_t x = pop_d128(ctx, "decimal::" #word);                                                          \
		return push_d128(ctx, "decimal::" #word, qd_d128_##word(x));                                                   \
	}

#define D128_BINARY(word)                                                                                              \
	int usr_decimal_##word(qd_context* ctx) {                                                                          \
		const qd_d128_t b = pop_d128(ctx, "decimal::" #word);                                                          \
		const qd_d128_t a = pop_d128(ctx, "decimal::" #word);                                                          \
		return push_d128(ctx, "decimal::" #word, qd_d128_##word(a, b));                                                \
	}

#define D128_PREDICATE(word)                                                                                           \
	int usr_decimal_##word(qd_context* ctx) {                                                                          \
		const qd_d128_t x = pop_d128(ctx, "decimal::" #word);                                                          \
		return push_i64(ctx, "decimal::" #word, qd_d128_##word(x) ? 1 : 0);                                            \
	}

#define D128_CONSTANT(word)                                                                                            \
	int usr_decimal_##word(qd_context* ctx) {                                                                          \
		return push_d128(ctx, "decimal::" #word, qd_d128_##word());                                                    \
	}

D128_UNARY(sqrt)
D128_UNARY(exp)
D128_UNARY(ln)
D128_UNARY(log10)
D128_BINARY(pow)
D128_UNARY(sin)
D128_UNARY(cos)
D128_UNARY(tan)
D128_UNARY(asin)
D128_UNARY(acos)
D128_UNARY(atan)
D128_BINARY(atan2)

D128_UNARY(abs)
D128_BINARY(copysign)
D128_BINARY(remainder)
D128_BINARY(fmod)
D128_BINARY(min)
D128_BINARY(max)
D128_UNARY(logb)
D128_UNARY(next_up)
D128_UNARY(next_down)
D128_BINARY(quantize)
D128_UNARY(reduce)

D128_UNARY(round_even)
D128_UNARY(round)
D128_UNARY(trunc)
D128_UNARY(floor)
D128_UNARY(ceil)

D128_PREDICATE(is_nan)
D128_PREDICATE(is_inf)
D128_PREDICATE(is_finite)
D128_PREDICATE(is_zero)
D128_PREDICATE(is_normal)
D128_PREDICATE(is_subnormal)
D128_PREDICATE(is_signed)

D128_CONSTANT(pi)
D128_CONSTANT(e)
D128_CONSTANT(inf)
D128_CONSTANT(nan)

int usr_decimal_fma(qd_context* ctx) {
	const qd_d128_t c = pop_d128(ctx, "decimal::fma");
	const qd_d128_t b = pop_d128(ctx, "decimal::fma");
	const qd_d128_t a = pop_d128(ctx, "decimal::fma");
	return push_d128(ctx, "decimal::fma", qd_d128_fma(a, b, c));
}

int usr_decimal_scaleb(qd_context* ctx) {
	const int64_t n = pop_i64(ctx, "decimal::scaleb");
	const qd_d128_t x = pop_d128(ctx, "decimal::scaleb");
	return push_d128(ctx, "decimal::scaleb", qd_d128_scaleb(x, n));
}

int usr_decimal_total_order(qd_context* ctx) {
	const qd_d128_t b = pop_d128(ctx, "decimal::total_order");
	const qd_d128_t a = pop_d128(ctx, "decimal::total_order");
	return push_i64(ctx, "decimal::total_order", qd_d128_total_order(a, b));
}
