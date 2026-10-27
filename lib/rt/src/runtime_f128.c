// The f128 side of the arithmetic, comparison and cast instructions.
//
// runtime_ops.c dispatches here once it has seen an f128 operand. f128 does not mix: an
// f128 meets only another f128, and a cast is how a value of another type becomes one.
// The compiler rejects the mixed cases, so reaching one here is a type error in code
// that went around it -- an `any` parameter, say.

#define _POSIX_C_SOURCE 200809L

#include <quadrate/rt/runtime.h>
#include "runtime_internal.h"

void qdrt_push_f128(qd_context* ctx, const char* op, qd_f128_t value) {
	if (qd_stack_push_f128(ctx->st, value) != QD_STACK_OK) {
		QDRT_FATAL(ctx, op, "Stack overflow pushing result");
	}
}

// Pops the two f128 operands of a binary instruction, checking what it can: `a` is the
// deeper one.
static void pop_two_f128(qd_context* ctx, const char* op, qd_f128_t* a, qd_f128_t* b) {
	QDRT_CHECK_STACK(ctx, op, 2);
	qd_stack* st = ctx->st;
	const qd_stack_type ta = st->data[st->size - 2].type;
	const qd_stack_type tb = st->data[st->size - 1].type;
	if (ta != QD_STACK_TYPE_F128 || tb != QD_STACK_TYPE_F128) {
		QDRT_FATAL(ctx, op, "Type error (f128 does not mix with %s; cast one side)",
				qdrt_type_name(ta == QD_STACK_TYPE_F128 ? tb : ta));
	}
	*b = st->data[--st->size].value.q;
	*a = st->data[--st->size].value.q;
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
