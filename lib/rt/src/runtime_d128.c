// The d128 side of the arithmetic, comparison and cast instructions.
//
// runtime_ops.c dispatches here once it has seen a d128 operand. A d128 meets another
// d128, or an i64, which widens to one exactly; an f64 does not mix, and a cast is how
// one becomes a d128. The compiler rejects that case, so reaching it here is a type
// error in code that went around it -- an `any` parameter, say.

#define _POSIX_C_SOURCE 200809L

#include "runtime_internal.h"
#include <quadrate/rt/runtime.h>

void qdrt_push_d128(qd_context* ctx, const char* op, qd_d128_t value) {
	if (qd_stack_push_d128(ctx->st, value) != QD_STACK_OK) {
		QDRT_FATAL(ctx, op, "Stack overflow pushing result");
	}
}

// An operand as a d128: one already, or an i64 widened to one
static bool as_d128(const qd_stack_element_t* e, qd_d128_t* out) {
	if (e->type == QD_STACK_TYPE_D128) {
		*out = e->value.d;
		return true;
	}
	if (e->type == QD_STACK_TYPE_INT) {
		*out = qd_d128_from_i64(e->value.i);
		return true;
	}
	return false;
}

// Pops the two operands of a binary instruction that has a d128 among them: `a` is the
// deeper one.
static void pop_two_d128(qd_context* ctx, const char* op, qd_d128_t* a, qd_d128_t* b) {
	QDRT_CHECK_STACK(ctx, op, 2);
	qd_stack* st = ctx->st;
	const qd_stack_element_t* ea = &st->data[st->size - 2];
	const qd_stack_element_t* eb = &st->data[st->size - 1];
	if (!as_d128(ea, a) || !as_d128(eb, b)) {
		QDRT_FATAL(ctx, op, "Type error (d128 does not mix with %s; cast one side)",
				qdrt_type_name(ea->type == QD_STACK_TYPE_D128 ? eb->type : ea->type));
	}
	st->size -= 2;
}

void qdrt_d128_binary(qd_context* ctx, const char* op, qd_d128_t (*fn)(qd_d128_t, qd_d128_t)) {
	qd_d128_t a;
	qd_d128_t b;
	pop_two_d128(ctx, op, &a, &b);
	qdrt_push_d128(ctx, op, fn(a, b));
}

void qdrt_d128_compare(qd_context* ctx, const char* op, bool (*test)(int order)) {
	qd_d128_t a;
	qd_d128_t b;
	pop_two_d128(ctx, op, &a, &b);
	if (qd_stack_push_int(ctx->st, test(qd_d128_compare(a, b)) ? 1 : 0) != QD_STACK_OK) {
		QDRT_FATAL(ctx, op, "Stack overflow pushing result");
	}
}

int qd_castd(qd_context* ctx) {
	QDRT_CHECK_STACK(ctx, "castd", 1);

	qd_stack_element_t elem;
	if (qd_stack_pop(ctx->st, &elem) != QD_STACK_OK) {
		QDRT_FATAL(ctx, "castd", "Failed to pop value");
	}

	qd_d128_t result;
	switch (elem.type) {
	case QD_STACK_TYPE_INT:
		result = qd_d128_from_i64(elem.value.i);
		break;
	case QD_STACK_TYPE_FLOAT:
		result = qd_d128_from_f64(elem.value.f);
		break;
	case QD_STACK_TYPE_D128:
		result = elem.value.d;
		break;
	default:
		qdrt_release_element(&elem);
		QDRT_FATAL(ctx, "castd", "Cannot cast %s to d128", qdrt_type_name(elem.type));
	}
	qdrt_push_d128(ctx, "castd", result);
	return (int){0};
}

int qd_cast_param(qd_context* ctx, int64_t depth, int64_t type) {
	QDRT_CHECK_STACK(ctx, "cast", (size_t)depth + 1);
	qd_stack_element_t* e = &ctx->st->data[ctx->st->size - 1 - (size_t)depth];
	if (e->type == (qd_stack_type)type) {
		return (int){0};
	}
	if (e->type == QD_STACK_TYPE_INT && type == QD_STACK_TYPE_D128) {
		e->value.d = qd_d128_from_i64(e->value.i);
	} else if (e->type == QD_STACK_TYPE_INT && type == QD_STACK_TYPE_FLOAT) {
		e->value.f = (double)e->value.i;
	} else if (e->type == QD_STACK_TYPE_FLOAT && type == QD_STACK_TYPE_INT) {
		e->value.i = (int64_t)e->value.f;
	} else {
		QDRT_FATAL(
				ctx, "cast", "Cannot convert %s to %s", qdrt_type_name(e->type), qdrt_type_name((qd_stack_type)type));
	}
	e->type = (qd_stack_type)type;
	return (int){0};
}
