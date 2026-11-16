// Arithmetic, comparison, and bitwise operations for Quadrate runtime
// Split from runtime.c for maintainability

#define _POSIX_C_SOURCE 200809L

#include "runtime_internal.h"
#include <inttypes.h>
#include <quadrate/rt/qd_string.h>
#include <quadrate/rt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Whether any of the top `n` stack elements is a d128. Those instructions go to
// runtime_d128.c, which also reports a d128 meeting anything else.
static inline bool d128_operand(const qd_stack* st, size_t n) {
	for (size_t i = 1; i <= n && i <= st->size; i++) {
		if (st->data[st->size - i].type == QD_STACK_TYPE_D128) {
			return true;
		}
	}
	return false;
}

static bool d128_eq(int order) {
	return order == 0;
}

// Unordered is "not equal": IEEE 754 compareQuietNotEqual is true when either side is NaN.
static bool d128_ne(int order) {
	return order != 0;
}

static bool d128_lt(int order) {
	return order == -1;
}

static bool d128_gt(int order) {
	return order == 1;
}

static bool d128_le(int order) {
	return order == -1 || order == 0;
}

static bool d128_ge(int order) {
	return order == 1 || order == 0;
}

// Binary arithmetic kinds for qdrt_arith_op.
typedef enum {
	QDRT_ARITH_ADD,
	QDRT_ARITH_SUB,
	QDRT_ARITH_MUL
} qdrt_arith_kind;

static inline int64_t qdrt_arith_int(qdrt_arith_kind kind, int64_t a, int64_t b) {
	switch (kind) {
	case QDRT_ARITH_ADD:
		return a + b;
	case QDRT_ARITH_SUB:
		return a - b;
	case QDRT_ARITH_MUL:
		return a * b;
	}
	return 0;
}

static inline double qdrt_arith_float(qdrt_arith_kind kind, double a, double b) {
	switch (kind) {
	case QDRT_ARITH_ADD:
		return a + b;
	case QDRT_ARITH_SUB:
		return a - b;
	case QDRT_ARITH_MUL:
		return a * b;
	}
	return 0.0;
}

// Shared implementation for +, - and *. Inlined into each caller so `kind` folds to a constant.
static inline int qdrt_arith_op(
		qd_context* ctx, const char* name, qdrt_arith_kind kind, qd_d128_t (*d128_op)(qd_d128_t, qd_d128_t)) {
	qd_stack* st = ctx->st;

	if (st->size >= 2) {
		qd_stack_element_t* top = &st->data[st->size - 1];
		qd_stack_element_t* second = &st->data[st->size - 2];

		if (top->type == QD_STACK_TYPE_INT && second->type == QD_STACK_TYPE_INT) {
			int64_t result = qdrt_arith_int(kind, second->value.i, top->value.i);
			st->size -= 2;
			QD_STACK_PUSH_INT_FAST(st, result);
			return (int){0};
		}
		if (qdrt_is_numeric_type(top->type) && qdrt_is_numeric_type(second->type)) {
			double result = qdrt_arith_float(kind, qdrt_to_double(second), qdrt_to_double(top));
			st->size -= 2;
			QD_STACK_PUSH_FLOAT_FAST(st, result);
			return (int){0};
		}
	}

	if (d128_operand(st, 2)) {
		qdrt_d128_binary(ctx, name, d128_op);
		return (int){0};
	}

	// Anything the fast path did not take is an underflow or a type error, which this raises.
	qdrt_validate_binary_numeric_op(ctx, name);
	return (int){-5};
}

int qd_add(qd_context* ctx) {
	return qdrt_arith_op(ctx, "add", QDRT_ARITH_ADD, qd_d128_add);
}

int qd_sub(qd_context* ctx) {
	return qdrt_arith_op(ctx, "sub", QDRT_ARITH_SUB, qd_d128_sub);
}

int qd_mul(qd_context* ctx) {
	return qdrt_arith_op(ctx, "mul", QDRT_ARITH_MUL, qd_d128_mul);
}

int qd_div(qd_context* ctx) {
	if (d128_operand(ctx->st, 2)) {
		qdrt_d128_binary(ctx, "div", qd_d128_div);
		return (int){0};
	}

	qdrt_validate_binary_numeric_op(ctx, "div");

	qd_stack_element_t a, b;
	int pop_result = qdrt_pop_two_values(ctx, &a, &b);
	if (pop_result != 0) {
		return pop_result;
	}

	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		if (b.value.i == 0) {
			QDRT_FATAL(ctx, "div", "Division by zero");
		}
		int64_t result = (b.value.i == -1) ? (int64_t)(0 - (uint64_t)a.value.i) : a.value.i / b.value.i;
		qd_stack_error err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			QDRT_FATAL(ctx, "div", "Stack overflow pushing result");
		}
	} else if (qdrt_is_numeric_type(a.type) && qdrt_is_numeric_type(b.type)) {
		double af = qdrt_to_double(&a);
		double bf = qdrt_to_double(&b);
		// No zero check: float division by zero is defined by IEEE 754 as +/-infinity, and
		// 0.0/0.0 as NaN. Only integer division by zero is an error. This path used to trap,
		// which contradicted the compiled one -- generator_nodes_instructions.cc emits a bare
		// fdiv for two doubles -- so the same expression aborted or returned `inf` depending
		// on whether the operands reached the inline path.
		double result = af / bf;
		qd_stack_error err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			QDRT_FATAL(ctx, "div", "Stack overflow pushing result");
		}
	} else {
		qdrt_release_element(&b);
		qdrt_release_element(&a);
		return (int){-5};
	}
	return (int){0};
}

int qd_mod(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		qdrt_fatal_raise(ctx, "mod", "Stack underflow (required 2 elements, have %zu)", stack_size);
	}

	// Check types
	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		qdrt_fatal_raise(ctx, "mod", "Failed to access stack elements");
	}

	if (check_a.type != QD_STACK_TYPE_INT || check_b.type != QD_STACK_TYPE_INT) {
		qdrt_fatal_raise(ctx, "mod", "Type error (expected int for modulo)");
	}

	qd_stack_element_t b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "mod", "Failed to pop value (stack changed unexpectedly)");
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "mod", "Failed to pop value (stack changed unexpectedly)");
	}

	if (b.value.i == 0) {
		QDRT_FATAL(ctx, "mod", "Division by zero");
	}

	int64_t result = (b.value.i == -1) ? 0 : a.value.i % b.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "mod", "Stack overflow pushing result");
	}

	return (int){0};
}

int qd_inc(qd_context* ctx) {
	// Pop one numeric value, add 1, push result with same type
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		qdrt_fatal_raise(ctx, "inc", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		qdrt_fatal_raise(ctx, "inc", "Failed to pop value");
	}

	if (elem.type == QD_STACK_TYPE_INT) {
		int64_t result = elem.value.i + 1;
		err = qd_stack_push_int(ctx->st, result);
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		double result = elem.value.f + 1.0;
		err = qd_stack_push_float(ctx->st, result);
	} else {
		qdrt_fatal_raise(ctx, "inc", "Invalid type (expected int or float)");
	}

	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "inc", "Stack operation failed");
	}

	return (int){0};
}

int qd_dec(qd_context* ctx) {
	// Pop one numeric value, subtract 1, push result with same type
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		qdrt_fatal_raise(ctx, "dec", "Stack underflow (requires 1 value)");
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		qdrt_fatal_raise(ctx, "dec", "Failed to pop value");
	}

	if (elem.type == QD_STACK_TYPE_INT) {
		int64_t result = elem.value.i - 1;
		err = qd_stack_push_int(ctx->st, result);
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		double result = elem.value.f - 1.0;
		err = qd_stack_push_float(ctx->st, result);
	} else {
		qdrt_fatal_raise(ctx, "dec", "Invalid type (expected int or float)");
	}

	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "dec", "Stack operation failed");
	}

	return (int){0};
}

int qd_neg(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		qdrt_fatal_raise(ctx, "neg", "Stack underflow (required 1 element, have %zu)", stack_size);
	}

	qd_stack_element_t check_val;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_val);
	if (check_err != QD_STACK_OK) {
		qdrt_fatal_raise(ctx, "neg", "Failed to access stack element");
	}

	if (check_val.type == QD_STACK_TYPE_D128) {
		qd_stack_element_t val;
		if (qd_stack_pop(ctx->st, &val) != QD_STACK_OK) {
			QDRT_FATAL(ctx, "neg", "Failed to pop value (stack changed unexpectedly)");
		}
		qdrt_push_d128(ctx, "neg", qd_d128_neg(val.value.d));
		return (int){0};
	}

	if (check_val.type != QD_STACK_TYPE_INT && check_val.type != QD_STACK_TYPE_FLOAT) {
		qdrt_fatal_raise(ctx, "neg", "Type error (expected numeric type)");
	}

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "neg", "Failed to pop value (stack changed unexpectedly)");
	}

	if (val.type == QD_STACK_TYPE_INT) {
		val.value.i = -val.value.i;
		err = qd_stack_push_int(ctx->st, val.value.i);
	} else {
		val.value.f = -val.value.f;
		err = qd_stack_push_float(ctx->st, val.value.f);
	}

	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "neg", "Stack overflow pushing result");
	}

	return (int){0};
}

// Shared implementation for == and !=. Supports string comparison, ptr/int
// null-checks, and numeric comparison. want_equal selects == (true) or !=.
static int qdrt_equality_op(qd_context* ctx, const char* name, bool want_equal) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		qdrt_fatal_raise(ctx, name, "Stack underflow (required 2 elements, have %zu)", stack_size);
	}

	if (d128_operand(ctx->st, 2)) {
		qdrt_d128_compare(ctx, name, want_equal ? d128_eq : d128_ne);
		return (int){0};
	}

	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		qdrt_fatal_raise(ctx, name, "Failed to access stack elements");
	}

	int is_string_compare = (check_a.type == QD_STACK_TYPE_STR && check_b.type == QD_STACK_TYPE_STR);

	// Two pointers compare by identity: same address, same thing. Structs are references, so
	// this is also how two struct values are asked whether they are the same struct -- "is this
	// child the last one", the question every walk over a linked structure ends on. It used to
	// be a fatal type error, so the question could not be written at all and callers went the
	// long way round (json::put asks `c <<next null ==` instead). Ordering pointers is a
	// different matter and stays rejected: `<` on two addresses means nothing a program should
	// rely on.
	int is_ptr_compare = (check_a.type == QD_STACK_TYPE_PTR && check_b.type == QD_STACK_TYPE_PTR);

	// Allow ptr compared with int (for null checks: ptr 0 == or ptr 0 !=)
	int is_ptr_null_check = (check_a.type == QD_STACK_TYPE_PTR && check_b.type == QD_STACK_TYPE_INT) ||
							(check_a.type == QD_STACK_TYPE_INT && check_b.type == QD_STACK_TYPE_PTR);

	if (!is_string_compare && !is_ptr_compare && !is_ptr_null_check &&
			((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
					(check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT))) {
		qdrt_fatal_raise(ctx, name, "Type error (expected numeric, string or pointer types for comparison)");
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "equality op", "Failed to pop value (stack changed unexpectedly)");
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "equality op", "Failed to pop value (stack changed unexpectedly)");
	}

	int equal;
	if (is_string_compare) {
		const char* str_a = qd_string_data(a.value.s);
		const char* str_b = qd_string_data(b.value.s);
		equal = (strcmp(str_a, str_b) == 0);
	} else if (is_ptr_compare) {
		equal = (a.value.p == b.value.p);
	} else if (is_ptr_null_check) {
		void* ptr_val = (a.type == QD_STACK_TYPE_PTR) ? a.value.p : b.value.p;
		int64_t int_val = (a.type == QD_STACK_TYPE_INT) ? a.value.i : b.value.i;
		equal = ((int64_t)(uintptr_t)ptr_val == int_val);
	} else {
		equal = (qdrt_to_double(&a) == qdrt_to_double(&b));
	}

	// Both operands were popped, so both references this call was handed have to go back.
	// The string path used to be the only one that did, and `node null ==` -- the test at the
	// head of every walk over a linked structure -- held on to the node for the rest of the
	// program: one leaked reference per comparison, which kept the node, and through it
	// everything the node pointed at.
	qdrt_release_element(&a);
	qdrt_release_element(&b);

	int64_t result = (equal == (int)want_equal) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "equality op", "Stack overflow pushing result");
	}

	return (int){0};
}

// Ordering comparison kinds for qdrt_order_op.
typedef enum {
	QDRT_ORDER_LT,
	QDRT_ORDER_GT,
	QDRT_ORDER_LTE,
	QDRT_ORDER_GTE
} qdrt_order_kind;

static bool (*const qdrt_order_d128[])(int) = {
		[QDRT_ORDER_LT] = d128_lt, [QDRT_ORDER_GT] = d128_gt, [QDRT_ORDER_LTE] = d128_le, [QDRT_ORDER_GTE] = d128_ge};

// Order of a against b: -1, 0 or 1, or 2 when unordered (a NaN is involved).
static inline int qdrt_order_cmp(const qd_stack_element_t* a, const qd_stack_element_t* b) {
	// Two ints compare exactly; through double they would collide above 2^53.
	if (a->type == QD_STACK_TYPE_INT && b->type == QD_STACK_TYPE_INT) {
		return (a->value.i > b->value.i) - (a->value.i < b->value.i);
	}
	double af = qdrt_to_double(a);
	double bf = qdrt_to_double(b);
	if (af < bf) {
		return -1;
	}
	if (af > bf) {
		return 1;
	}
	return (af == bf) ? 0 : 2;
}

// Shared implementation for the numeric-only ordering operators (<, >, <=, >=).
static int qdrt_order_op(qd_context* ctx, const char* name, qdrt_order_kind kind) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		qdrt_fatal_raise(ctx, name, "Stack underflow (required 2 elements, have %zu)", stack_size);
	}

	if (d128_operand(ctx->st, 2)) {
		qdrt_d128_compare(ctx, name, qdrt_order_d128[kind]);
		return (int){0};
	}

	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		qdrt_fatal_raise(ctx, name, "Failed to access stack elements");
	}

	if (!qdrt_is_numeric_type(check_a.type) || !qdrt_is_numeric_type(check_b.type)) {
		qdrt_fatal_raise(ctx, name, "Type error (expected numeric types for comparison)");
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "comparison op", "Failed to pop value (stack changed unexpectedly)");
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "comparison op", "Failed to pop value (stack changed unexpectedly)");
	}

	int64_t result = qdrt_order_d128[kind](qdrt_order_cmp(&a, &b)) ? 1 : 0;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "comparison op", "Stack overflow pushing result");
	}

	return (int){0};
}

// Bitwise binary operation kinds for qdrt_bitwise_op.
typedef enum {
	QDRT_BIT_AND,
	QDRT_BIT_OR,
	QDRT_BIT_XOR
} qdrt_bitwise_kind;

// Shared implementation for the integer bitwise operators (&, |, ^).
static int qdrt_bitwise_op(qd_context* ctx, const char* name, qdrt_bitwise_kind kind) {
	qd_stack* st = ctx->st;
	if (st->size < 2) {
		qdrt_fatal_raise(ctx, name, "Stack underflow (required 2 elements, have %zu)", (size_t)st->size);
	}

	qd_stack_element_t* top = &st->data[st->size - 1];
	qd_stack_element_t* second = &st->data[st->size - 2];

	if (top->type != QD_STACK_TYPE_INT || second->type != QD_STACK_TYPE_INT) {
		qdrt_fatal_raise(ctx, name, "Type error (expected int for bitwise operation)");
	}

	int64_t result = 0;
	switch (kind) {
	case QDRT_BIT_AND:
		result = second->value.i & top->value.i;
		break;
	case QDRT_BIT_OR:
		result = second->value.i | top->value.i;
		break;
	case QDRT_BIT_XOR:
		result = second->value.i ^ top->value.i;
		break;
	}
	st->size -= 2;
	QD_STACK_PUSH_INT_FAST(st, result);
	return (int){0};
}

// Shared implementation for the shift operators. left selects << (true) or >>.
static int qdrt_shift_op(qd_context* ctx, const char* name, bool left) {
	qd_stack* st = ctx->st;
	if (st->size < 2) {
		qdrt_fatal_raise(ctx, name, "Stack underflow (required 2 elements, have %zu)", (size_t)st->size);
	}

	qd_stack_element_t* top = &st->data[st->size - 1];
	qd_stack_element_t* second = &st->data[st->size - 2];

	if (top->type != QD_STACK_TYPE_INT || second->type != QD_STACK_TYPE_INT) {
		qdrt_fatal_raise(ctx, name, "Type error (expected int for shift operation)");
	}

	if (top->value.i < 0 || top->value.i >= 64) {
		qdrt_fatal_raise(ctx, name, "Shift count out of range (must be 0-63, got %" PRId64 ")", top->value.i);
	}

	int64_t result = left ? (int64_t)((uint64_t)second->value.i << top->value.i)
						  : (int64_t)((uint64_t)second->value.i >> top->value.i);
	st->size -= 2;
	QD_STACK_PUSH_INT_FAST(st, result);
	return (int){0};
}

int qd_eq(qd_context* ctx) {
	return qdrt_equality_op(ctx, "eq", true);
}

int qd_neq(qd_context* ctx) {
	return qdrt_equality_op(ctx, "neq", false);
}

int qd_lt(qd_context* ctx) {
	return qdrt_order_op(ctx, "lt", QDRT_ORDER_LT);
}

int qd_gt(qd_context* ctx) {
	return qdrt_order_op(ctx, "gt", QDRT_ORDER_GT);
}

int qd_lte(qd_context* ctx) {
	return qdrt_order_op(ctx, "lte", QDRT_ORDER_LTE);
}

int qd_gte(qd_context* ctx) {
	return qdrt_order_op(ctx, "gte", QDRT_ORDER_GTE);
}

int qd_within(qd_context* ctx) {
	// Check we have at least 3 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		qdrt_fatal_raise(ctx, "within", "Stack underflow (required 3 elements, have %zu)", stack_size);
	}

	// Check all three are numeric types
	qd_stack_element_t check_max, check_min, check_value;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_max);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_min);
	}
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 3, &check_value);
	}
	if (check_err != QD_STACK_OK) {
		qdrt_fatal_raise(ctx, "within", "Failed to access stack elements");
	}

	if ((check_value.type != QD_STACK_TYPE_INT && check_value.type != QD_STACK_TYPE_FLOAT) ||
			(check_min.type != QD_STACK_TYPE_INT && check_min.type != QD_STACK_TYPE_FLOAT) ||
			(check_max.type != QD_STACK_TYPE_INT && check_max.type != QD_STACK_TYPE_FLOAT)) {
		qdrt_fatal_raise(ctx, "within", "Type error (expected numeric types for comparison)");
	}

	// Pop the three values
	qd_stack_element_t max, min, value;
	qd_stack_error err = qd_stack_pop(ctx->st, &max);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "within", "Failed to pop value (stack changed unexpectedly)");
	}
	err = qd_stack_pop(ctx->st, &min);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "within", "Failed to pop value (stack changed unexpectedly)");
	}
	err = qd_stack_pop(ctx->st, &value);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "within", "Failed to pop value (stack changed unexpectedly)");
	}

	// Convert to double for comparison
	double value_f = qdrt_to_double(&value);
	double min_f = qdrt_to_double(&min);
	double max_f = qdrt_to_double(&max);

	// Check if value is within [min, max]
	int64_t result = (value_f >= min_f && value_f <= max_f) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "within", "Stack overflow pushing result");
	}

	return (int){0};
}

int qd_and(qd_context* ctx) {
	return qdrt_bitwise_op(ctx, "and", QDRT_BIT_AND);
}

int qd_or(qd_context* ctx) {
	return qdrt_bitwise_op(ctx, "or", QDRT_BIT_OR);
}

int qd_xor(qd_context* ctx) {
	return qdrt_bitwise_op(ctx, "xor", QDRT_BIT_XOR);
}

int qd_not(qd_context* ctx) {
	qd_stack* st = ctx->st;
	if (st->size < 1) {
		qdrt_fatal_raise(ctx, "not", "Stack underflow (required 1 element, have %zu)", (size_t)st->size);
	}

	qd_stack_element_t* top = &st->data[st->size - 1];

	if (top->type != QD_STACK_TYPE_INT) {
		qdrt_fatal_raise(ctx, "not", "Type error (expected int for bitwise operation)");
	}

	top->value.i = ~top->value.i;
	return (int){0};
}

// Logical AND and OR, as distinct from the bitwise ops above: every non-zero value counts
// as true, so `2 1 and` is 1 where the bitwise `bits::and` makes it 0. Both operands are
// evaluated -- these are ordinary stack words, not short-circuiting forms.
static int qdrt_logical_op(qd_context* ctx, const char* name, bool isOr) {
	qd_stack* st = ctx->st;
	if (st->size < 2) {
		qdrt_fatal_raise(ctx, name, "Stack underflow (required 2 elements, have %zu)", (size_t)st->size);
	}

	qd_stack_element_t* b = &st->data[st->size - 1];
	qd_stack_element_t* a = &st->data[st->size - 2];

	if (a->type != QD_STACK_TYPE_INT || b->type != QD_STACK_TYPE_INT) {
		qdrt_fatal_raise(ctx, name, "Type error (expected int for logical operation)");
	}

	bool left = a->value.i != 0;
	bool right = b->value.i != 0;
	a->value.i = (isOr ? (left || right) : (left && right)) ? 1 : 0;
	st->size--;
	return (int){0};
}

int qd_land(qd_context* ctx) {
	return qdrt_logical_op(ctx, "and", false);
}

int qd_lor(qd_context* ctx) {
	return qdrt_logical_op(ctx, "or", true);
}

// Logical negation, as distinct from the bitwise `not` above: 0 -> 1, anything else -> 0.
int qd_lnot(qd_context* ctx) {
	qd_stack* st = ctx->st;
	if (st->size < 1) {
		qdrt_fatal_raise(ctx, "lnot", "Stack underflow (required 1 element, have %zu)", (size_t)st->size);
	}

	qd_stack_element_t* top = &st->data[st->size - 1];

	if (top->type != QD_STACK_TYPE_INT) {
		qdrt_fatal_raise(ctx, "lnot", "Type error (expected int for logical operation)");
	}

	top->value.i = (top->value.i == 0) ? 1 : 0;
	return (int){0};
}

// A fallible call's status is 0 on success and the error code on failure. `ok` and `fail`
// turn it into a flag for `if`, `while` and `until`.
static int qdrt_status_test(qd_context* ctx, const char* name, bool want_ok) {
	qd_stack* st = ctx->st;
	if (st->size < 1) {
		qdrt_fatal_raise(ctx, name, "Stack underflow (required 1 element, have %zu)", (size_t)st->size);
	}

	qd_stack_element_t* top = &st->data[st->size - 1];

	if (top->type != QD_STACK_TYPE_INT) {
		qdrt_fatal_raise(ctx, name, "Type error (expected an int status)");
	}

	top->value.i = ((top->value.i == 0) == want_ok) ? 1 : 0;
	return (int){0};
}

int qd_status_ok(qd_context* ctx) {
	return qdrt_status_test(ctx, "ok", true);
}

int qd_status_fail(qd_context* ctx) {
	return qdrt_status_test(ctx, "fail", false);
}

int qd_shl(qd_context* ctx) {
	return qdrt_shift_op(ctx, "bits::shl", true);
}

int qd_shr(qd_context* ctx) {
	return qdrt_shift_op(ctx, "bits::shr", false);
}
