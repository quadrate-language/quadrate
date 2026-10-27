// f128:: -- the stack words over the runtime's decimal128 functions (quadrate/rt/f128.h).
//
// The arithmetic is the runtime's; each word here pops its operands, calls the function of
// the same name and pushes what it returns. The compiler has already checked that every
// operand is an f128, so an operand that is not one is a program that went around it -- an
// `any` parameter -- and is fatal rather than something to recover from.

#include <quadrate/f128/f128.h>
#include <quadrate/rt/f128.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <stdint.h>

// An i64 argument widens to an f128 exactly, as it does in arithmetic
static qd_f128_t pop_f128(qd_context* ctx, const char* word) {
	qd_stack_element_t elem;
	if (qd_stack_pop(ctx->st, &elem) != QD_STACK_OK) {
		qd_fatal_raise(ctx, word, "Stack underflow");
	}
	if (elem.type == QD_STACK_TYPE_INT) {
		return qd_f128_from_i64(elem.value.i);
	}
	if (elem.type != QD_STACK_TYPE_F128) {
		qd_fatal_raise(ctx, word, "Type error (expected f128)");
	}
	return elem.value.q;
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

static int push_f128(qd_context* ctx, const char* word, qd_f128_t value) {
	if (qd_stack_push_f128(ctx->st, value) != QD_STACK_OK) {
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

#define F128_UNARY(word)                                                                                               \
	int usr_f128_##word(qd_context* ctx) {                                                                             \
		const qd_f128_t x = pop_f128(ctx, "f128::" #word);                                                             \
		return push_f128(ctx, "f128::" #word, qd_f128_##word(x));                                                      \
	}

#define F128_BINARY(word)                                                                                              \
	int usr_f128_##word(qd_context* ctx) {                                                                             \
		const qd_f128_t b = pop_f128(ctx, "f128::" #word);                                                             \
		const qd_f128_t a = pop_f128(ctx, "f128::" #word);                                                             \
		return push_f128(ctx, "f128::" #word, qd_f128_##word(a, b));                                                   \
	}

#define F128_PREDICATE(word)                                                                                           \
	int usr_f128_##word(qd_context* ctx) {                                                                             \
		const qd_f128_t x = pop_f128(ctx, "f128::" #word);                                                             \
		return push_i64(ctx, "f128::" #word, qd_f128_##word(x) ? 1 : 0);                                              \
	}

#define F128_CONSTANT(word)                                                                                            \
	int usr_f128_##word(qd_context* ctx) {                                                                             \
		return push_f128(ctx, "f128::" #word, qd_f128_##word());                                                       \
	}

F128_UNARY(sqrt)
F128_UNARY(exp)
F128_UNARY(ln)
F128_UNARY(log10)
F128_BINARY(pow)
F128_UNARY(sin)
F128_UNARY(cos)
F128_UNARY(tan)
F128_UNARY(asin)
F128_UNARY(acos)
F128_UNARY(atan)
F128_BINARY(atan2)

F128_UNARY(abs)
F128_BINARY(copysign)
F128_BINARY(remainder)
F128_BINARY(fmod)
F128_BINARY(min)
F128_BINARY(max)
F128_UNARY(logb)
F128_UNARY(next_up)
F128_UNARY(next_down)
F128_BINARY(quantize)
F128_UNARY(reduce)

F128_UNARY(round_even)
F128_UNARY(round)
F128_UNARY(trunc)
F128_UNARY(floor)
F128_UNARY(ceil)

F128_PREDICATE(is_nan)
F128_PREDICATE(is_inf)
F128_PREDICATE(is_finite)
F128_PREDICATE(is_zero)
F128_PREDICATE(is_normal)
F128_PREDICATE(is_subnormal)
F128_PREDICATE(is_signed)

F128_CONSTANT(pi)
F128_CONSTANT(e)
F128_CONSTANT(inf)
F128_CONSTANT(nan)

int usr_f128_fma(qd_context* ctx) {
	const qd_f128_t c = pop_f128(ctx, "f128::fma");
	const qd_f128_t b = pop_f128(ctx, "f128::fma");
	const qd_f128_t a = pop_f128(ctx, "f128::fma");
	return push_f128(ctx, "f128::fma", qd_f128_fma(a, b, c));
}

int usr_f128_scaleb(qd_context* ctx) {
	const int64_t n = pop_i64(ctx, "f128::scaleb");
	const qd_f128_t x = pop_f128(ctx, "f128::scaleb");
	return push_f128(ctx, "f128::scaleb", qd_f128_scaleb(x, n));
}

int usr_f128_total_order(qd_context* ctx) {
	const qd_f128_t b = pop_f128(ctx, "f128::total_order");
	const qd_f128_t a = pop_f128(ctx, "f128::total_order");
	return push_i64(ctx, "f128::total_order", qd_f128_total_order(a, b));
}
