// Arithmetic, comparison, and bitwise operations for Quadrate runtime
// Split from runtime.c for maintainability

#define _POSIX_C_SOURCE 200809L

#include <quadrate/rt/runtime.h>
#include <quadrate/rt/qd_string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime_internal.h"

int qd_add(qd_context* ctx) {
	qd_stack* st = ctx->st;

	// Fast path: check stack has 2 elements and both are integers
	if (st->size >= 2) {
		qd_stack_element_t* top = &st->data[st->size - 1];
		qd_stack_element_t* second = &st->data[st->size - 2];

		if (top->type == QD_STACK_TYPE_INT && second->type == QD_STACK_TYPE_INT) {
			// Fast integer addition - most common case
			int64_t result = second->value.i + top->value.i;
			st->size -= 2;
			QD_STACK_PUSH_INT_FAST(st, result);
			return (int){0};
		}
		// Fast float path
		if (qdrt_is_numeric_type(top->type) && qdrt_is_numeric_type(second->type)) {
			double a_val = (second->type == QD_STACK_TYPE_INT) ? (double)second->value.i : second->value.f;
			double b_val = (top->type == QD_STACK_TYPE_INT) ? (double)top->value.i : top->value.f;
			st->size -= 2;
			QD_STACK_PUSH_FLOAT_FAST(st, a_val + b_val);
			return (int){0};
		}
	}

	// Slow path with full error checking
	qdrt_validate_binary_numeric_op(ctx, "add");

	qd_stack_element_t a, b;
	int pop_result = qdrt_pop_two_values(ctx, &a, &b);
	if (pop_result != 0) {
		return pop_result;
	}

	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		int64_t result = a.value.i + b.value.i;
		qd_stack_error err = qd_stack_push_int(st, result);
		if (err != QD_STACK_OK) {
			return (int){-2};
		}
	} else if (qdrt_is_numeric_type(a.type) && qdrt_is_numeric_type(b.type)) {
		double result = qdrt_to_double(&a) + qdrt_to_double(&b);
		qd_stack_error err = qd_stack_push_float(st, result);
		if (err != QD_STACK_OK) {
			return (int){-2};
		}
	} else {
		qdrt_release_if_string(&b);
		qdrt_release_if_string(&a);
		return (int){-5};
	}
	return (int){0};
}

int qd_sub(qd_context* ctx) {
	qd_stack* st = ctx->st;

	// Fast path: check stack has 2 elements and both are integers
	if (st->size >= 2) {
		qd_stack_element_t* top = &st->data[st->size - 1];
		qd_stack_element_t* second = &st->data[st->size - 2];

		if (top->type == QD_STACK_TYPE_INT && second->type == QD_STACK_TYPE_INT) {
			// Fast integer subtraction - most common case
			int64_t result = second->value.i - top->value.i;
			st->size -= 2;
			QD_STACK_PUSH_INT_FAST(st, result);
			return (int){0};
		}
		// Fast float path
		if (qdrt_is_numeric_type(top->type) && qdrt_is_numeric_type(second->type)) {
			double a_val = (second->type == QD_STACK_TYPE_INT) ? (double)second->value.i : second->value.f;
			double b_val = (top->type == QD_STACK_TYPE_INT) ? (double)top->value.i : top->value.f;
			st->size -= 2;
			QD_STACK_PUSH_FLOAT_FAST(st, a_val - b_val);
			return (int){0};
		}
	}

	// Slow path with full error checking
	qdrt_validate_binary_numeric_op(ctx, "sub");

	qd_stack_element_t a, b;
	int pop_result = qdrt_pop_two_values(ctx, &a, &b);
	if (pop_result != 0) {
		return pop_result;
	}

	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		int64_t result = a.value.i - b.value.i;
		qd_stack_error err = qd_stack_push_int(st, result);
		if (err != QD_STACK_OK) {
			return (int){-2};
		}
	} else if (qdrt_is_numeric_type(a.type) && qdrt_is_numeric_type(b.type)) {
		double result = qdrt_to_double(&a) - qdrt_to_double(&b);
		qd_stack_error err = qd_stack_push_float(st, result);
		if (err != QD_STACK_OK) {
			return (int){-2};
		}
	} else {
		qdrt_release_if_string(&b);
		qdrt_release_if_string(&a);
		return (int){-5};
	}
	return (int){0};
}

int qd_mul(qd_context* ctx) {
	qdrt_validate_binary_numeric_op(ctx, "mul");

	qd_stack_element_t a, b;
	int pop_result = qdrt_pop_two_values(ctx, &a, &b);
	if (pop_result != 0) {
		return pop_result;
	}

	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		int64_t result = a.value.i * b.value.i;
		qd_stack_error err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (int){-2};
		}
	} else if (qdrt_is_numeric_type(a.type) && qdrt_is_numeric_type(b.type)) {
		double result = qdrt_to_double(&a) * qdrt_to_double(&b);
		qd_stack_error err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (int){-2};
		}
	} else {
		qdrt_release_if_string(&b);
		qdrt_release_if_string(&a);
		return (int){-5};
	}
	return (int){0};
}

int qd_div(qd_context* ctx) {
	qdrt_validate_binary_numeric_op(ctx, "div");

	qd_stack_element_t a, b;
	int pop_result = qdrt_pop_two_values(ctx, &a, &b);
	if (pop_result != 0) {
		return pop_result;
	}

	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		if (b.value.i == 0) {
			fprintf(stderr, "Fatal error in div: Division by zero\n");
			qdrt_dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
		}
		int64_t result = a.value.i / b.value.i;
		qd_stack_error err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (int){-2};
		}
	} else if (qdrt_is_numeric_type(a.type) && qdrt_is_numeric_type(b.type)) {
		double af = qdrt_to_double(&a);
		double bf = qdrt_to_double(&b);
		if (bf == 0.0) {
			fprintf(stderr, "Fatal error in div: Division by zero\n");
			qdrt_dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
		}
		double result = af / bf;
		qd_stack_error err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (int){-2};
		}
	} else {
		qdrt_release_if_string(&b);
		qdrt_release_if_string(&a);
		return (int){-5};
	}
	return (int){0};
}

int qd_mod(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in mod: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check types
	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in mod: Failed to access stack elements\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_a.type != QD_STACK_TYPE_INT || check_b.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in mod: Type error (expected int for modulo)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	if (b.value.i == 0) {
		fprintf(stderr, "Fatal error in mod: Division by zero\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t result = a.value.i % b.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int qd_inc(qd_context* ctx) {
	// Pop one numeric value, add 1, push result with same type
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in inc: Stack underflow (requires 1 value)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in inc: Failed to pop value\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type == QD_STACK_TYPE_INT) {
		int64_t result = elem.value.i + 1;
		err = qd_stack_push_int(ctx->st, result);
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		double result = elem.value.f + 1.0;
		err = qd_stack_push_float(ctx->st, result);
	} else {
		fprintf(stderr, "Fatal error in inc: Invalid type (expected int or float)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int qd_dec(qd_context* ctx) {
	// Pop one numeric value, subtract 1, push result with same type
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in dec: Stack underflow (requires 1 value)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dec: Failed to pop value\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type == QD_STACK_TYPE_INT) {
		int64_t result = elem.value.i - 1;
		err = qd_stack_push_int(ctx->st, result);
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		double result = elem.value.f - 1.0;
		err = qd_stack_push_float(ctx->st, result);
	} else {
		fprintf(stderr, "Fatal error in dec: Invalid type (expected int or float)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int qd_neg(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in neg: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_val;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_val);
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in neg: Failed to access stack element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_val.type != QD_STACK_TYPE_INT && check_val.type != QD_STACK_TYPE_FLOAT) {
		fprintf(stderr, "Fatal error in neg: Type error (expected numeric type)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	if (val.type == QD_STACK_TYPE_INT) {
		val.value.i = -val.value.i;
		err = qd_stack_push_int(ctx->st, val.value.i);
	} else {
		val.value.f = -val.value.f;
		err = qd_stack_push_float(ctx->st, val.value.f);
	}

	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int qd_eq(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in eq: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check both are numeric types
	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in eq: Failed to access stack elements\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check for string comparison
	int is_string_compare = (check_a.type == QD_STACK_TYPE_STR && check_b.type == QD_STACK_TYPE_STR);

	// Allow ptr compared with int (for null checks: ptr 0 == or ptr 0 !=)
	int is_ptr_null_check = (check_a.type == QD_STACK_TYPE_PTR && check_b.type == QD_STACK_TYPE_INT) ||
	                        (check_a.type == QD_STACK_TYPE_INT && check_b.type == QD_STACK_TYPE_PTR);

	if (!is_string_compare && !is_ptr_null_check && ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT))) {
		fprintf(stderr, "Fatal error in eq: Type error (expected numeric or string types for comparison)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	int64_t result;
	if (is_string_compare) {
		// Compare strings
		const char* str_a = qd_string_data(a.value.s);
		const char* str_b = qd_string_data(b.value.s);
		result = (strcmp(str_a, str_b) == 0) ? 1 : 0;
		qd_string_release(a.value.s);
		qd_string_release(b.value.s);
	} else if (is_ptr_null_check) {
		// Compare pointer to integer (null check)
		void* ptr_val = (a.type == QD_STACK_TYPE_PTR) ? a.value.p : b.value.p;
		int64_t int_val = (a.type == QD_STACK_TYPE_INT) ? a.value.i : b.value.i;
		result = ((int64_t)(uintptr_t)ptr_val == int_val) ? 1 : 0;
	} else {
		// Convert to double for comparison
		double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
		double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;
		result = (af == bf) ? 1 : 0;
	}
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int qd_neq(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in neq: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check both are numeric types
	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in neq: Failed to access stack elements\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check for string comparison
	int is_string_compare = (check_a.type == QD_STACK_TYPE_STR && check_b.type == QD_STACK_TYPE_STR);

	// Allow ptr compared with int (for null checks: ptr 0 == or ptr 0 !=)
	int is_ptr_null_check = (check_a.type == QD_STACK_TYPE_PTR && check_b.type == QD_STACK_TYPE_INT) ||
	                        (check_a.type == QD_STACK_TYPE_INT && check_b.type == QD_STACK_TYPE_PTR);

	if (!is_string_compare && !is_ptr_null_check && ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT))) {
		fprintf(stderr, "Fatal error in neq: Type error (expected numeric or string types for comparison)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	int64_t result;
	if (is_string_compare) {
		// Compare strings
		const char* str_a = qd_string_data(a.value.s);
		const char* str_b = qd_string_data(b.value.s);
		result = (strcmp(str_a, str_b) != 0) ? 1 : 0;
		qd_string_release(a.value.s);
		qd_string_release(b.value.s);
	} else if (is_ptr_null_check) {
		// Compare pointer to integer (null check)
		void* ptr_val = (a.type == QD_STACK_TYPE_PTR) ? a.value.p : b.value.p;
		int64_t int_val = (a.type == QD_STACK_TYPE_INT) ? a.value.i : b.value.i;
		result = ((int64_t)(uintptr_t)ptr_val != int_val) ? 1 : 0;
	} else {
		// Convert to double for comparison
		double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
		double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;
		result = (af != bf) ? 1 : 0;
	}
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int qd_lt(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in lt: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check both are numeric types
	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in lt: Failed to access stack elements\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in lt: Type error (expected numeric types for comparison)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	// Convert to double for comparison
	double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;

	int64_t result = (af < bf) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int qd_gt(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in gt: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check both are numeric types
	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in gt: Failed to access stack elements\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in gt: Type error (expected numeric types for comparison)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	// Convert to double for comparison
	double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;

	int64_t result = (af > bf) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int qd_lte(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in lte: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check both are numeric types
	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in lte: Failed to access stack elements\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in lte: Type error (expected numeric types for comparison)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	// Convert to double for comparison
	double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;

	int64_t result = (af <= bf) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int qd_gte(qd_context* ctx) {
	qd_stack* st = ctx->st;

	// Fast path: check stack has 2 elements and both are integers
	if (QD_STACK_SIZE(st) >= 2) {
		qd_stack_element_t* top = &st->data[st->size - 1];
		qd_stack_element_t* second = &st->data[st->size - 2];

		if (top->type == QD_STACK_TYPE_INT && second->type == QD_STACK_TYPE_INT) {
			// Fast integer comparison - most common case
			int64_t b = top->value.i;
			int64_t a = second->value.i;
			st->size -= 2;
			QD_STACK_PUSH_INT_FAST(st, (a >= b) ? 1 : 0);
			return (int){0};
		}

		if ((top->type == QD_STACK_TYPE_INT || top->type == QD_STACK_TYPE_FLOAT) &&
		    (second->type == QD_STACK_TYPE_INT || second->type == QD_STACK_TYPE_FLOAT)) {
			// Float comparison path
			double bf = (top->type == QD_STACK_TYPE_INT) ? (double)top->value.i : top->value.f;
			double af = (second->type == QD_STACK_TYPE_INT) ? (double)second->value.i : second->value.f;
			st->size -= 2;
			QD_STACK_PUSH_INT_FAST(st, (af >= bf) ? 1 : 0);
			return (int){0};
		}

		// Type error
		fprintf(stderr, "Fatal error in gte: Type error (expected numeric types for comparison)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Stack underflow
	fprintf(stderr, "Fatal error in gte: Stack underflow (required 2 elements, have %zu)\n", QD_STACK_SIZE(st));
	qdrt_dump_stack(ctx);
	qd_print_stack_trace(ctx);
	abort();
}

int qd_within(qd_context* ctx) {
	// Check we have at least 3 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in within: Stack underflow (required 3 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
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
		fprintf(stderr, "Fatal error in within: Failed to access stack elements\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_value.type != QD_STACK_TYPE_INT && check_value.type != QD_STACK_TYPE_FLOAT) ||
	    (check_min.type != QD_STACK_TYPE_INT && check_min.type != QD_STACK_TYPE_FLOAT) ||
	    (check_max.type != QD_STACK_TYPE_INT && check_max.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in within: Type error (expected numeric types for comparison)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the three values
	qd_stack_element_t max, min, value;
	qd_stack_error err = qd_stack_pop(ctx->st, &max);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	err = qd_stack_pop(ctx->st, &min);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	err = qd_stack_pop(ctx->st, &value);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	// Convert to double for comparison
	double value_f = (value.type == QD_STACK_TYPE_INT) ? (double)value.value.i : value.value.f;
	double min_f = (min.type == QD_STACK_TYPE_INT) ? (double)min.value.i : min.value.f;
	double max_f = (max.type == QD_STACK_TYPE_INT) ? (double)max.value.i : max.value.f;

	// Check if value is within [min, max]
	int64_t result = (value_f >= min_f && value_f <= max_f) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

int qd_and(qd_context* ctx) {
	qd_stack* st = ctx->st;
	if (st->size < 2) {
		fprintf(stderr, "Fatal error in and: Stack underflow (required 2 elements, have %zu)\n", st->size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t* top = &st->data[st->size - 1];
	qd_stack_element_t* second = &st->data[st->size - 2];

	if (top->type != QD_STACK_TYPE_INT || second->type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in and: Type error (expected int for bitwise operation)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t result = second->value.i & top->value.i;
	st->size -= 2;
	QD_STACK_PUSH_INT_FAST(st, result);
	return (int){0};
}

int qd_or(qd_context* ctx) {
	qd_stack* st = ctx->st;
	if (st->size < 2) {
		fprintf(stderr, "Fatal error in or: Stack underflow (required 2 elements, have %zu)\n", st->size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t* top = &st->data[st->size - 1];
	qd_stack_element_t* second = &st->data[st->size - 2];

	if (top->type != QD_STACK_TYPE_INT || second->type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in or: Type error (expected int for bitwise operation)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t result = second->value.i | top->value.i;
	st->size -= 2;
	QD_STACK_PUSH_INT_FAST(st, result);
	return (int){0};
}

int qd_xor(qd_context* ctx) {
	qd_stack* st = ctx->st;
	if (st->size < 2) {
		fprintf(stderr, "Fatal error in xor: Stack underflow (required 2 elements, have %zu)\n", st->size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t* top = &st->data[st->size - 1];
	qd_stack_element_t* second = &st->data[st->size - 2];

	if (top->type != QD_STACK_TYPE_INT || second->type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in xor: Type error (expected int for bitwise operation)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t result = second->value.i ^ top->value.i;
	st->size -= 2;
	QD_STACK_PUSH_INT_FAST(st, result);
	return (int){0};
}

int qd_not(qd_context* ctx) {
	qd_stack* st = ctx->st;
	if (st->size < 1) {
		fprintf(stderr, "Fatal error in not: Stack underflow (required 1 element, have %zu)\n", st->size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t* top = &st->data[st->size - 1];

	if (top->type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in not: Type error (expected int for bitwise operation)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	top->value.i = ~top->value.i;
	return (int){0};
}

int qd_shl(qd_context* ctx) {
	qd_stack* st = ctx->st;
	if (st->size < 2) {
		fprintf(stderr, "Fatal error in shl: Stack underflow (required 2 elements, have %zu)\n", st->size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t* top = &st->data[st->size - 1];
	qd_stack_element_t* second = &st->data[st->size - 2];

	if (top->type != QD_STACK_TYPE_INT || second->type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in shl: Type error (expected int for shift operation)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (top->value.i < 0 || top->value.i >= 64) {
		fprintf(stderr, "Fatal error in shl: Shift count out of range (must be 0-63, got %ld)\n", top->value.i);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t result = (int64_t)((uint64_t)second->value.i << top->value.i);
	st->size -= 2;
	QD_STACK_PUSH_INT_FAST(st, result);
	return (int){0};
}

int qd_shr(qd_context* ctx) {
	qd_stack* st = ctx->st;
	if (st->size < 2) {
		fprintf(stderr, "Fatal error in shr: Stack underflow (required 2 elements, have %zu)\n", st->size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t* top = &st->data[st->size - 1];
	qd_stack_element_t* second = &st->data[st->size - 2];

	if (top->type != QD_STACK_TYPE_INT || second->type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in shr: Type error (expected int for shift operation)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (top->value.i < 0 || top->value.i >= 64) {
		fprintf(stderr, "Fatal error in shr: Shift count out of range (must be 0-63, got %ld)\n", top->value.i);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Logical shift right (unsigned)
	int64_t result = (int64_t)((uint64_t)second->value.i >> top->value.i);
	st->size -= 2;
	QD_STACK_PUSH_INT_FAST(st, result);
	return (int){0};
}

