// The f128 side of the arithmetic, comparison and cast instructions.
//
// runtime_ops.c dispatches here once it has seen an f128 operand. An f128 meets another
// f128, or an i64, which widens to one exactly; an f64 does not mix, and a cast is how
// one becomes an f128. The compiler rejects that case, so reaching it here is a type
// error in code that went around it -- an `any` parameter, say.

#define _POSIX_C_SOURCE 200809L

#include <quadrate/rt/runtime.h>
#include "runtime_internal.h"

void qdrt_push_f128(qd_context* ctx, const char* op, qd_f128_t value) {
	if (qd_stack_push_f128(ctx->st, value) != QD_STACK_OK) {
		QDRT_FATAL(ctx, op, "Stack overflow pushing result");
	}
}

// An operand as an f128: one already, or an i64 widened to one
static bool as_f128(const qd_stack_element_t* e, qd_f128_t* out) {
	if (e->type == QD_STACK_TYPE_F128) {
		*out = e->value.q;
		return true;
	}
	if (e->type == QD_STACK_TYPE_INT) {
		*out = qd_f128_from_i64(e->value.i);
		return true;
	}
	return false;
}

// Pops the two operands of a binary instruction that has an f128 among them: `a` is the
// deeper one.
static void pop_two_f128(qd_context* ctx, const char* op, qd_f128_t* a, qd_f128_t* b) {
	QDRT_CHECK_STACK(ctx, op, 2);
	qd_stack* st = ctx->st;
	const qd_stack_element_t* ea = &st->data[st->size - 2];
	const qd_stack_element_t* eb = &st->data[st->size - 1];
	if (!as_f128(ea, a) || !as_f128(eb, b)) {
		QDRT_FATAL(ctx, op, "Type error (f128 does not mix with %s; cast one side)",
				qdrt_type_name(ea->type == QD_STACK_TYPE_F128 ? eb->type : ea->type));
	}
	st->size -= 2;
}

void qdrt_f128_binary(qd_context* ctx, const char* op, qd_f128_t (*fn)(qd_f128_t, qd_f128_t)) {
	qd_f128_t a;
	qd_f128_t b;
	pop_two_f128(ctx, op, &a, &b);
	qdrt_push_f128(ctx, op, fn(a, b));
}

void qdrt_f128_compare(qd_context* ctx, const char* op, bool (*test)(int order)) {
	qd_f128_t a;
	qd_f128_t b;
	pop_two_f128(ctx, op, &a, &b);
	if (qd_stack_push_int(ctx->st, test(qd_f128_compare(a, b)) ? 1 : 0) != QD_STACK_OK) {
		QDRT_FATAL(ctx, op, "Stack overflow pushing result");
	}
}

int qd_castq(qd_context* ctx) {
	QDRT_CHECK_STACK(ctx, "castq", 1);

	qd_stack_element_t elem;
	if (qd_stack_pop(ctx->st, &elem) != QD_STACK_OK) {
		QDRT_FATAL(ctx, "castq", "Failed to pop value");
	}

	qd_f128_t result;
	switch (elem.type) {
		case QD_STACK_TYPE_INT:
			result = qd_f128_from_i64(elem.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			result = qd_f128_from_f64(elem.value.f);
			break;
		case QD_STACK_TYPE_F128:
			result = elem.value.q;
			break;
		default:
			qdrt_release_element(&elem);
			QDRT_FATAL(ctx, "castq", "Cannot cast %s to f128", qdrt_type_name(elem.type));
	}
	qdrt_push_f128(ctx, "castq", result);
	return (int){0};
}

int qd_cast_param(qd_context* ctx, int64_t depth, int64_t type) {
	QDRT_CHECK_STACK(ctx, "cast", (size_t)depth + 1);
	qd_stack_element_t* e = &ctx->st->data[ctx->st->size - 1 - (size_t)depth];
	if (e->type == (qd_stack_type)type) {
		return (int){0};
	}
	if (e->type == QD_STACK_TYPE_INT && type == QD_STACK_TYPE_F128) {
		e->value.q = qd_f128_from_i64(e->value.i);
	} else if (e->type == QD_STACK_TYPE_INT && type == QD_STACK_TYPE_FLOAT) {
		e->value.f = (double)e->value.i;
	} else if (e->type == QD_STACK_TYPE_FLOAT && type == QD_STACK_TYPE_INT) {
		e->value.i = (int64_t)e->value.f;
	} else {
		QDRT_FATAL(ctx, "cast", "Cannot convert %s to %s", qdrt_type_name(e->type),
				qdrt_type_name((qd_stack_type)type));
	}
	e->type = (qd_stack_type)type;
	return (int){0};
}
