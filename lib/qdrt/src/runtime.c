#define _POSIX_C_SOURCE 200809L

#include <qdrt/runtime.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

static void dump_stack(qd_context* ctx);

qd_exec_result qd_push_i(qd_context* ctx, int64_t value) {
	qd_stack_error err = qd_stack_push_int(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_push_f(qd_context* ctx, double value) {
	qd_stack_error err = qd_stack_push_float(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_push_s(qd_context* ctx, const char* value) {
	qd_stack_error err = qd_stack_push_str(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_push_p(qd_context* ctx, void* value) {
	qd_stack_error err = qd_stack_push_ptr(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_print(qd_context* ctx) {
	// Pop and print the top element
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	switch (val.type) {
		case QD_STACK_TYPE_INT:
			printf("%ld", val.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			printf("%g", val.value.f);
			break;
		case QD_STACK_TYPE_STR:
			printf("%s", val.value.s);
			free(val.value.s);  // Free the string memory after printing
			break;
		default:
			return (qd_exec_result){-3};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_nl(qd_context* ctx) {
	(void)ctx;  // Unused parameter
	printf("\n");
	return (qd_exec_result){0};
}

// Helper function to check if string contains whitespace
static bool has_whitespace(const char* str) {
	for (const char* p = str; *p; p++) {
		if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
			return true;
		}
	}
	return false;
}

qd_exec_result qd_prints(qd_context* ctx) {
	// Print entire stack (non-destructive) - output only values for piping
	const size_t stack_size = qd_stack_size(ctx->st);

	// Print from bottom to top, all on one line
	for (size_t i = 0; i < stack_size; i++) {
		qd_stack_element_t val;
		qd_stack_error err = qd_stack_element(ctx->st, i, &val);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}

		if (i > 0) {
			printf(" ");
		}

		switch (val.type) {
			case QD_STACK_TYPE_INT:
				printf("%ld", val.value.i);
				break;
			case QD_STACK_TYPE_FLOAT:
				printf("%g", val.value.f);
				break;
			case QD_STACK_TYPE_STR:
				// Smart quoting: only quote if string contains whitespace
				if (has_whitespace(val.value.s)) {
					printf("\"%s\"", val.value.s);
				} else {
					printf("%s", val.value.s);
				}
				break;
			default:
				return (qd_exec_result){-3};
		}
	}

	if (stack_size > 0) {
		printf("\n");
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_printv(qd_context* ctx) {
	// Forth-style verbose: pop and print the top element with type info
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	switch (val.type) {
		case QD_STACK_TYPE_INT:
			printf("int:%ld\n", val.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			printf("float:%g\n", val.value.f);
			break;
		case QD_STACK_TYPE_STR:
			// Smart quoting: only quote if string contains whitespace
			if (has_whitespace(val.value.s)) {
				printf("string:\"%s\"\n", val.value.s);
			} else {
				printf("string:%s\n", val.value.s);
			}
			free(val.value.s);  // Free the string memory after printing
			break;
		default:
			return (qd_exec_result){-3};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_printsv(qd_context* ctx) {
	// Print entire stack with type info (non-destructive)
	const size_t stack_size = qd_stack_size(ctx->st);

	// Print from bottom to top, all on one line
	for (size_t i = 0; i < stack_size; i++) {
		qd_stack_element_t val;
		qd_stack_error err = qd_stack_element(ctx->st, i, &val);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}

		if (i > 0) {
			printf(" ");
		}

		switch (val.type) {
			case QD_STACK_TYPE_INT:
				printf("int:%ld", val.value.i);
				break;
			case QD_STACK_TYPE_FLOAT:
				printf("float:%g", val.value.f);
				break;
			case QD_STACK_TYPE_STR:
				// Smart quoting: only quote if string contains whitespace
				if (has_whitespace(val.value.s)) {
					printf("string:\"%s\"", val.value.s);
				} else {
					printf("string:%s", val.value.s);
				}
				break;
			case QD_STACK_TYPE_PTR:
				printf("ptr:%p", val.value.p);
				break;
			default:
				return (qd_exec_result){-3};
		}
	}

	if (stack_size > 0) {
		printf("\n");
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_peek(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_peek(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	switch (val.type) {
		case QD_STACK_TYPE_INT:
			printf("%ld\n", val.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			printf("%f\n", val.value.f);
			break;
		case QD_STACK_TYPE_STR:
			printf("%s\n", val.value.s);
			break;
		default:
			return (qd_exec_result){-3};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_err_push(qd_context* ctx, qd_stack_error value) {
	(void)ctx;
	(void)value;
	return (qd_exec_result){0};
}

// Helper function to check if a stack type is numeric
static inline bool is_numeric_type(qd_stack_type type) {
	return type == QD_STACK_TYPE_INT || type == QD_STACK_TYPE_FLOAT;
}

// Helper function to validate binary numeric operation setup
// Returns true if validation passed, false if error occurred (and already aborted)
static bool validate_binary_numeric_op(qd_context* ctx, const char* op_name) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in %s: Stack underflow (required 2 elements, have %zu)\n",
		        op_name, stack_size);
		dump_stack(ctx);
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
		fprintf(stderr, "Fatal error in %s: Failed to access stack elements\n", op_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (!is_numeric_type(check_a.type) || !is_numeric_type(check_b.type)) {
		fprintf(stderr, "Fatal error in %s: Type error (expected numeric types)\n", op_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	return true;
}

// Helper function to pop two values from stack for binary operations
static qd_exec_result pop_two_values(qd_context* ctx, qd_stack_element_t* a, qd_stack_element_t* b) {
	qd_stack_error err = qd_stack_pop(ctx->st, b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

// Helper function to convert stack element to double
static inline double to_double(const qd_stack_element_t* elem) {
	return (elem->type == QD_STACK_TYPE_INT) ? (double)elem->value.i : elem->value.f;
}

// Helper function to free string values if needed
static void free_if_string(qd_stack_element_t* elem) {
	if (elem->type == QD_STACK_TYPE_STR) {
		free(elem->value.s);
	}
}

qd_exec_result qd_div(qd_context* ctx) {
	validate_binary_numeric_op(ctx, "div");

	qd_stack_element_t a, b;
	qd_exec_result pop_result = pop_two_values(ctx, &a, &b);
	if (pop_result.code != 0) {
		return pop_result;
	}

	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		if (b.value.i == 0) {
			return (qd_exec_result){-4};
		}
		int64_t result = a.value.i / b.value.i;
		qd_stack_error err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else if (is_numeric_type(a.type) && is_numeric_type(b.type)) {
		double af = to_double(&a);
		double bf = to_double(&b);
		if (bf == 0.0) {
			return (qd_exec_result){-4};
		}
		double result = af / bf;
		qd_stack_error err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else {
		free_if_string(&b);
		free_if_string(&a);
		return (qd_exec_result){-5};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_mul(qd_context* ctx) {
	validate_binary_numeric_op(ctx, "mul");

	qd_stack_element_t a, b;
	qd_exec_result pop_result = pop_two_values(ctx, &a, &b);
	if (pop_result.code != 0) {
		return pop_result;
	}

	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		int64_t result = a.value.i * b.value.i;
		qd_stack_error err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else if (is_numeric_type(a.type) && is_numeric_type(b.type)) {
		double result = to_double(&a) * to_double(&b);
		qd_stack_error err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else {
		free_if_string(&b);
		free_if_string(&a);
		return (qd_exec_result){-5};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_add(qd_context* ctx) {
	validate_binary_numeric_op(ctx, "add");

	qd_stack_element_t a, b;
	qd_exec_result pop_result = pop_two_values(ctx, &a, &b);
	if (pop_result.code != 0) {
		return pop_result;
	}

	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		int64_t result = a.value.i + b.value.i;
		qd_stack_error err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else if (is_numeric_type(a.type) && is_numeric_type(b.type)) {
		double result = to_double(&a) + to_double(&b);
		qd_stack_error err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else {
		free_if_string(&b);
		free_if_string(&a);
		return (qd_exec_result){-5};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_sub(qd_context* ctx) {
	validate_binary_numeric_op(ctx, "sub");

	qd_stack_element_t a, b;
	qd_exec_result pop_result = pop_two_values(ctx, &a, &b);
	if (pop_result.code != 0) {
		return pop_result;
	}

	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		int64_t result = a.value.i - b.value.i;
		qd_stack_error err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else if (is_numeric_type(a.type) && is_numeric_type(b.type)) {
		double result = to_double(&a) - to_double(&b);
		qd_stack_error err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else {
		free_if_string(&b);
		free_if_string(&a);
		return (qd_exec_result){-5};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_dup(qd_context* ctx) {
	// Duplicate the top element of the stack
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in dup: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t top;
	qd_stack_error err = qd_stack_peek(ctx->st, &top);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dup: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push a copy of the top element
	switch (top.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, top.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, top.value.f);
			break;
		case QD_STACK_TYPE_STR:
			// Need to duplicate the string
			err = qd_stack_push_str(ctx->st, top.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, top.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_dupd(qd_context* ctx) {
	// Duplicate the second element of the stack: ( a b -- a a b )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in dupd: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop top two elements
	qd_stack_element_t a, b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);  // b is top
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dupd: Failed to pop top element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	err = qd_stack_pop(ctx->st, &a);  // a is second
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dupd: Failed to pop second element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push back: a, a, b
	// Push first a
	switch (a.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, a.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, a.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, a.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, a.value.p);
			break;
		default:
			fprintf(stderr, "Fatal error in dupd: Unknown type for second element\n");
			dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
	}
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dupd: Failed to push first copy of second element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push second a (duplicate)
	switch (a.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, a.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, a.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, a.value.s);
			free(a.value.s);  // Free the original since push_str makes a copy
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, a.value.p);
			break;
		default:
			fprintf(stderr, "Fatal error in dupd: Unknown type for second element\n");
			dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
	}
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dupd: Failed to push second copy of second element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push b (top element)
	switch (b.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, b.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, b.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, b.value.s);
			free(b.value.s);  // Free the original since push_str makes a copy
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, b.value.p);
			break;
		default:
			fprintf(stderr, "Fatal error in dupd: Unknown type for top element\n");
			dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
	}
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dupd: Failed to push top element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_dup2(qd_context* ctx) {
	// Duplicate the top two elements of the stack: ( a b -- a b a b )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in dup2: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the second element (index stack_size - 2)
	qd_stack_element_t second;
	qd_stack_error err = qd_stack_element(ctx->st, stack_size - 2, &second);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dup2: Failed to access second element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the top element (index stack_size - 1)
	qd_stack_element_t top;
	err = qd_stack_element(ctx->st, stack_size - 1, &top);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dup2: Failed to access top element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push a copy of the second element
	switch (second.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, second.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, second.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, second.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, second.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push a copy of the top element
	switch (top.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, top.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, top.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, top.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, top.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_swapd(qd_context* ctx) {
	// Swap second and third elements: ( a b c -- b a c )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in swapd: Stack underflow (required 3 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop top three elements
	qd_stack_element_t a, b, c;
	qd_stack_error err = qd_stack_pop(ctx->st, &c);  // c is top
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in swapd: Failed to pop top element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	err = qd_stack_pop(ctx->st, &b);  // b is second
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in swapd: Failed to pop second element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	err = qd_stack_pop(ctx->st, &a);  // a is third
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in swapd: Failed to pop third element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push back: b, a, c (swapped second and third)
	// Push b
	switch (b.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, b.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, b.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, b.value.s);
			free(b.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, b.value.p);
			break;
		default:
			fprintf(stderr, "Fatal error in swapd: Unknown type for second element\n");
			dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
	}
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in swapd: Failed to push b\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push a
	switch (a.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, a.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, a.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, a.value.s);
			free(a.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, a.value.p);
			break;
		default:
			fprintf(stderr, "Fatal error in swapd: Unknown type for third element\n");
			dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
	}
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in swapd: Failed to push a\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push c
	switch (c.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, c.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, c.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, c.value.s);
			free(c.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, c.value.p);
			break;
		default:
			fprintf(stderr, "Fatal error in swapd: Unknown type for top element\n");
			dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
	}
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in swapd: Failed to push c\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_overd(qd_context* ctx) {
	// Copy third element between second and top: ( a b c -- a b a c )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in overd: Stack underflow (required 3 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the top element
	qd_stack_element_t top;
	qd_stack_error err = qd_stack_pop(ctx->st, &top);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in overd: Failed to pop top element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the third element (now at index stack_size - 3, but stack is smaller by 1)
	qd_stack_element_t third;
	err = qd_stack_element(ctx->st, stack_size - 3, &third);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in overd: Failed to access third element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push a copy of the third element
	switch (third.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, third.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, third.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, third.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, third.value.p);
			break;
		default:
			fprintf(stderr, "Fatal error in overd: Unknown type for third element\n");
			dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
	}
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in overd: Failed to push copy of third element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push the top element back
	switch (top.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, top.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, top.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, top.value.s);
			free(top.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, top.value.p);
			break;
		default:
			fprintf(stderr, "Fatal error in overd: Unknown type for top element\n");
			dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
	}
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in overd: Failed to push top element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_nipd(qd_context* ctx) {
	// Remove second element: ( a b c -- a c )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in nipd: Stack underflow (required 3 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop top two elements
	qd_stack_element_t b, c;
	qd_stack_error err = qd_stack_pop(ctx->st, &c);  // c is top
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in nipd: Failed to pop top element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	err = qd_stack_pop(ctx->st, &b);  // b is second (to be removed)
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in nipd: Failed to pop second element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Free b's resources if it's a string
	if (b.type == QD_STACK_TYPE_STR) {
		free(b.value.s);
	}

	// Push c back
	switch (c.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, c.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, c.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, c.value.s);
			free(c.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, c.value.p);
			break;
		default:
			fprintf(stderr, "Fatal error in nipd: Unknown type for top element\n");
			dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
	}
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in nipd: Failed to push top element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_swap(qd_context* ctx) {
	// Swap the top two elements of the stack
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in swap: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop top two elements
	qd_stack_element_t a, b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);  // b is top
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &a);  // a is second
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push them back in swapped order (b first, then a)
	// Push b (was top, now will be second)
	switch (b.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, b.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, b.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, b.value.s);
			free(b.value.s);  // Free the original since push_str makes a copy
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, b.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push a (was second, now will be top)
	switch (a.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, a.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, a.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, a.value.s);
			free(a.value.s);  // Free the original since push_str makes a copy
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, a.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_over(qd_context* ctx) {
	// Copy the second element to the top: ( a b -- a b a )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in over: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the second element (without popping)
	qd_stack_element_t second;
	qd_stack_error err = qd_stack_element(ctx->st, stack_size - 2, &second);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in over: Failed to access second element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push a copy of the second element to the top
	switch (second.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, second.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, second.value.f);
			break;
		case QD_STACK_TYPE_STR:
			// Need to duplicate the string
			err = qd_stack_push_str(ctx->st, second.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, second.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_nip(qd_context* ctx) {
	// Remove the second element: ( a b -- b )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in nip: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the top element
	qd_stack_element_t top;
	qd_stack_error err = qd_stack_pop(ctx->st, &top);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Pop the second element (which we want to discard)
	qd_stack_element_t second;
	err = qd_stack_pop(ctx->st, &second);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Free string memory if necessary
	if (second.type == QD_STACK_TYPE_STR) {
		free(second.value.s);
	}

	// Push the top element back
	switch (top.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, top.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, top.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, top.value.s);
			free(top.value.s);  // Free the original since push_str makes a copy
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, top.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// sqrt - square root

// cb - cube (x^3)

// cbrt - cube root

// ceil - ceiling (round up)

// floor - floor (round down)

// ln - natural logarithm (base e)

// log10 - base 10 logarithm

// call - invoke function pointer from stack
qd_exec_result qd_call(qd_context* ctx) {
	// Pop function pointer and call it
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in call: Stack underflow\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Verify it's a pointer type
	if (val.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in call: Expected pointer type, got %d\n", val.type);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Cast to function pointer and call it
	// Function signature: qd_exec_result (*)(qd_context*)
	// Use memcpy to avoid pedantic warnings about object-to-function pointer conversion
	typedef qd_exec_result (*qd_function_ptr)(qd_context*);
	qd_function_ptr func;
	memcpy(&func, &val.value.p, sizeof(func));

	if (func == NULL) {
		fprintf(stderr, "Fatal error in call: NULL function pointer\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Call the function
	return func(ctx);
}

// dec - decrement (subtract 1, preserves type)
qd_exec_result qd_dec(qd_context* ctx) {
	// Pop one numeric value, subtract 1, push result with same type
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in dec: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dec: Failed to pop value\n");
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// inc - increment (add 1, preserves type)
qd_exec_result qd_inc(qd_context* ctx) {
	// Pop one numeric value, add 1, push result with same type
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in inc: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in inc: Failed to pop value\n");
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// casti - cast top stack element to integer
qd_exec_result qd_casti(qd_context* ctx) {
	// Pop one value, convert to integer, push result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in casti: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in casti: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t result;
	if (elem.type == QD_STACK_TYPE_INT) {
		result = elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		result = (int64_t)elem.value.f;
	} else if (elem.type == QD_STACK_TYPE_STR) {
		result = atoll(elem.value.s);
		free(elem.value.s);  // Free the string after conversion
	} else {
		fprintf(stderr, "Fatal error in casti: Cannot cast type to integer\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// castf - cast top stack element to float
qd_exec_result qd_castf(qd_context* ctx) {
	// Pop one value, convert to float, push result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in castf: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in castf: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result;
	if (elem.type == QD_STACK_TYPE_INT) {
		result = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		result = elem.value.f;
	} else if (elem.type == QD_STACK_TYPE_STR) {
		result = atof(elem.value.s);
		free(elem.value.s);  // Free the string after conversion
	} else {
		fprintf(stderr, "Fatal error in castf: Cannot cast type to float\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// casts - cast top stack element to string
qd_exec_result qd_casts(qd_context* ctx) {
	// Pop one value, convert to string, push result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in casts: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in casts: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	char buffer[64];
	if (elem.type == QD_STACK_TYPE_INT) {
		snprintf(buffer, sizeof(buffer), "%ld", elem.value.i);
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		snprintf(buffer, sizeof(buffer), "%g", elem.value.f);
	} else if (elem.type == QD_STACK_TYPE_STR) {
		err = qd_stack_push_str(ctx->st, elem.value.s);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
		return (qd_exec_result){0};
	} else {
		fprintf(stderr, "Fatal error in casts: Cannot cast type to string\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_push_str(ctx->st, buffer);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// pow - exponentiation (base^exponent)

// round - round to nearest integer

// clear - empty the entire stack
qd_exec_result qd_clear(qd_context* ctx) {
	// Pop all elements from the stack until empty
	qd_stack_element_t elem;
	while (!qd_stack_is_empty(ctx->st)) {
		qd_stack_error err = qd_stack_pop(ctx->st, &elem);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "Fatal error in clear: Failed to pop element\n");
			dump_stack(ctx);
			abort();
		}
		// Free string memory if it was a string element
		if (elem.type == QD_STACK_TYPE_STR) {
			free(elem.value.s);
		}
	}

	return (qd_exec_result){0};
}

// depth - push the current stack depth onto the stack
qd_exec_result qd_depth(qd_context* ctx) {
	// Get current stack size and push it as an integer
	size_t stack_size = qd_stack_size(ctx->st);

	qd_stack_error err = qd_stack_push_int(ctx->st, (int64_t)stack_size);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// fac - factorial (n!)

// inv - inverse/reciprocal (1/x, returns float)

// eq - equal (==) comparison: ( a b -- result )
// Pops two values, pushes 1 if equal, 0 otherwise
qd_exec_result qd_eq(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in eq: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in eq: Type error (expected numeric types for comparison)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Convert to double for comparison
	double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;

	int64_t result = (af == bf) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// neq - not equal (!=) comparison: ( a b -- result )
// Pops two values, pushes 1 if not equal, 0 otherwise
qd_exec_result qd_neq(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in neq: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in neq: Type error (expected numeric types for comparison)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Convert to double for comparison
	double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;

	int64_t result = (af != bf) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// lt - less than (<) comparison: ( a b -- result )
// Pops two values, pushes 1 if a < b, 0 otherwise
qd_exec_result qd_lt(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in lt: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in lt: Type error (expected numeric types for comparison)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Convert to double for comparison
	double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;

	int64_t result = (af < bf) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// gt - greater than (>) comparison: ( a b -- result )
// Pops two values, pushes 1 if a > b, 0 otherwise
qd_exec_result qd_gt(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in gt: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in gt: Type error (expected numeric types for comparison)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Convert to double for comparison
	double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;

	int64_t result = (af > bf) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// lte - less than or equal (<=) comparison: ( a b -- result )
// Pops two values, pushes 1 if a <= b, 0 otherwise
qd_exec_result qd_lte(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in lte: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in lte: Type error (expected numeric types for comparison)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Convert to double for comparison
	double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;

	int64_t result = (af <= bf) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// gte - greater than or equal (>=) comparison: ( a b -- result )
// Pops two values, pushes 1 if a >= b, 0 otherwise
qd_exec_result qd_gte(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in gte: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
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
		fprintf(stderr, "Fatal error in gte: Failed to access stack elements\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in gte: Type error (expected numeric types for comparison)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	qd_stack_element_t a;
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Convert to double for comparison
	double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;

	int64_t result = (af >= bf) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// within - check if value is within range [min, max]: ( value min max -- result )
// Pops three values, pushes 1 if min <= value <= max, 0 otherwise
qd_exec_result qd_within(qd_context* ctx) {
	// Check we have at least 3 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in within: Stack underflow (required 3 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_value.type != QD_STACK_TYPE_INT && check_value.type != QD_STACK_TYPE_FLOAT) ||
	    (check_min.type != QD_STACK_TYPE_INT && check_min.type != QD_STACK_TYPE_FLOAT) ||
	    (check_max.type != QD_STACK_TYPE_INT && check_max.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in within: Type error (expected numeric types for comparison)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the three values
	qd_stack_element_t max, min, value;
	qd_stack_error err = qd_stack_pop(ctx->st, &max);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &min);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Convert to double for comparison
	double value_f = (value.type == QD_STACK_TYPE_INT) ? (double)value.value.i : value.value.f;
	double min_f = (min.type == QD_STACK_TYPE_INT) ? (double)min.value.i : min.value.f;
	double max_f = (max.type == QD_STACK_TYPE_INT) ? (double)max.value.i : max.value.f;

	// Check if value is within [min, max]
	int64_t result = (value_f >= min_f && value_f <= max_f) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// Dump current stack contents for debugging
static void dump_stack(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	fprintf(stderr, "\nStack dump (%zu elements):\n", stack_size);

	if (stack_size == 0) {
		fprintf(stderr, "  (empty)\n");
		return;
	}

	for (size_t i = 0; i < stack_size; i++) {
		qd_stack_element_t elem;
		qd_stack_error err = qd_stack_element(ctx->st, i, &elem);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "  [%zu]: <error reading element>\n", i);
			continue;
		}

		fprintf(stderr, "  [%zu]: ", i);
		switch (elem.type) {
			case QD_STACK_TYPE_INT:
				fprintf(stderr, "int = %ld\n", elem.value.i);
				break;
			case QD_STACK_TYPE_FLOAT:
				fprintf(stderr, "float = %f\n", elem.value.f);
				break;
			case QD_STACK_TYPE_STR:
				fprintf(stderr, "str = \"%s\"\n", elem.value.s);
				break;
			case QD_STACK_TYPE_PTR:
				fprintf(stderr, "ptr = %p\n", elem.value.p);
				break;
			default:
				fprintf(stderr, "<unknown type>\n");
				break;
		}
	}
}

void qd_check_stack(qd_context* ctx, size_t count, const qd_stack_type* types, const char* func_name) {
	// Check stack has enough elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < count) {
		fprintf(stderr, "Fatal error in %s: Stack underflow (required %zu elements, have %zu)\n",
			func_name, count, stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check types match (from bottom to top of required elements)
	for (size_t i = 0; i < count; i++) {
		// Skip type check for untyped parameters (marked with QD_STACK_TYPE_PTR)
		if (types[i] == QD_STACK_TYPE_PTR) {
			continue;
		}

		qd_stack_element_t elem;
		size_t stack_index = stack_size - count + i;
		qd_stack_error err = qd_stack_element(ctx->st, stack_index, &elem);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "Fatal error in %s: Failed to access stack element at index %zu\n",
				func_name, stack_index);
			abort();
		}

		if (elem.type != types[i]) {
			const char* expected_type_name = "";
			const char* actual_type_name = "";

			switch (types[i]) {
				case QD_STACK_TYPE_INT: expected_type_name = "int"; break;
				case QD_STACK_TYPE_FLOAT: expected_type_name = "float"; break;
				case QD_STACK_TYPE_STR: expected_type_name = "str"; break;
				case QD_STACK_TYPE_PTR: expected_type_name = "ptr"; break;
			}

			switch (elem.type) {
				case QD_STACK_TYPE_INT: actual_type_name = "int"; break;
				case QD_STACK_TYPE_FLOAT: actual_type_name = "float"; break;
				case QD_STACK_TYPE_STR: actual_type_name = "str"; break;
				case QD_STACK_TYPE_PTR: actual_type_name = "ptr"; break;
			}

			fprintf(stderr, "Fatal error in %s: Type mismatch for parameter %zu (expected %s, got %s)\n",
				func_name, i + 1, expected_type_name, actual_type_name);
			dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
		}
	}
}

// drop - remove top element from stack: ( a -- )
qd_exec_result qd_drop(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in drop: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Free string if needed
	if (val.type == QD_STACK_TYPE_STR) {
		free(val.value.s);
	}

	return (qd_exec_result){0};
}

// drop2 - remove top 2 elements from stack: ( a b -- )
qd_exec_result qd_drop2(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in drop2: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t val;
	// Drop first element
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	if (val.type == QD_STACK_TYPE_STR) {
		free(val.value.s);
	}

	// Drop second element
	err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	if (val.type == QD_STACK_TYPE_STR) {
		free(val.value.s);
	}

	return (qd_exec_result){0};
}

// rot - rotate top 3 elements: ( a b c -- b c a )
qd_exec_result qd_rot(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in rot: Stack underflow (required 3 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop c, b, a
	qd_stack_element_t c, b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &c);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push in order: b, c, a
	// Push b
	switch (b.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, b.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, b.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, b.value.s);
			free(b.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, b.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push c
	switch (c.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, c.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, c.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, c.value.s);
			free(c.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, c.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push a
	switch (a.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, a.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, a.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, a.value.s);
			free(a.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, a.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// tuck - insert copy of top below second: ( a b -- b a b )
qd_exec_result qd_tuck(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in tuck: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop b and a
	qd_stack_element_t b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push in order: b, a, b
	// Push b (first copy)
	switch (b.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, b.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, b.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, b.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, b.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}
	if (err != QD_STACK_OK) {
		if (b.type == QD_STACK_TYPE_STR) free(b.value.s);
		if (a.type == QD_STACK_TYPE_STR) free(a.value.s);
		return (qd_exec_result){-2};
	}

	// Push a
	switch (a.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, a.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, a.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, a.value.s);
			free(a.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, a.value.p);
			break;
		default:
			if (b.type == QD_STACK_TYPE_STR) free(b.value.s);
			return (qd_exec_result){-3};
	}
	if (err != QD_STACK_OK) {
		if (b.type == QD_STACK_TYPE_STR) free(b.value.s);
		return (qd_exec_result){-2};
	}

	// Push b (second copy)
	switch (b.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, b.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, b.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, b.value.s);
			free(b.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, b.value.p);
			break;
		default:
			if (b.type == QD_STACK_TYPE_STR) free(b.value.s);
			return (qd_exec_result){-3};
	}
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// pick - copy nth element to top (0-indexed from top): ( ... n -- ... nth )
qd_exec_result qd_pick(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in pick: Stack underflow (need at least the index)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the index
	qd_stack_element_t idx_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &idx_elem);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	if (idx_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in pick: Index must be an integer\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t n = idx_elem.value.i;
	if (n < 0) {
		fprintf(stderr, "Fatal error in pick: Index must be non-negative (got %ld)\n", n);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	stack_size = qd_stack_size(ctx->st);  // Update after popping index
	if ((size_t)n >= stack_size) {
		fprintf(stderr, "Fatal error in pick: Index %ld out of range (stack has %zu elements)\n", n, stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the nth element from the top (0 = top)
	qd_stack_element_t elem;
	err = qd_stack_element(ctx->st, stack_size - 1 - (size_t)n, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in pick: Failed to access element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push a copy of that element
	switch (elem.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, elem.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, elem.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, elem.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, elem.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// roll - rotate n elements, moving nth to top: ( ... n -- ... )
qd_exec_result qd_roll(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in roll: Stack underflow (need at least the count)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the count
	qd_stack_element_t count_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &count_elem);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	if (count_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in roll: Count must be an integer\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t n = count_elem.value.i;
	if (n < 0) {
		fprintf(stderr, "Fatal error in roll: Count must be non-negative (got %ld)\n", n);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (n == 0) {
		return (qd_exec_result){0};  // Nothing to do
	}

	stack_size = qd_stack_size(ctx->st);  // Update after popping count
	if ((size_t)n > stack_size) {
		fprintf(stderr, "Fatal error in roll: Count %ld exceeds stack size %zu\n", n, stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Allocate temporary storage for n elements
	qd_stack_element_t* temp = malloc(sizeof(qd_stack_element_t) * (size_t)n);
	if (!temp) {
		fprintf(stderr, "Fatal error in roll: Memory allocation failed\n");
		abort();
	}

	// Pop n elements
	for (int64_t i = 0; i < n; i++) {
		err = qd_stack_pop(ctx->st, &temp[n - 1 - i]);
		if (err != QD_STACK_OK) {
			free(temp);
			return (qd_exec_result){-2};
		}
	}

	// Push back in rotated order: the bottom element (temp[0]) goes to top
	// Order after: temp[1], temp[2], ..., temp[n-1], temp[0]
	for (int64_t i = 1; i < n; i++) {
		switch (temp[i].type) {
			case QD_STACK_TYPE_INT:
				err = qd_stack_push_int(ctx->st, temp[i].value.i);
				break;
			case QD_STACK_TYPE_FLOAT:
				err = qd_stack_push_float(ctx->st, temp[i].value.f);
				break;
			case QD_STACK_TYPE_STR:
				err = qd_stack_push_str(ctx->st, temp[i].value.s);
				free(temp[i].value.s);
				break;
			case QD_STACK_TYPE_PTR:
				err = qd_stack_push_ptr(ctx->st, temp[i].value.p);
				break;
			default:
				free(temp);
				return (qd_exec_result){-3};
		}
		if (err != QD_STACK_OK) {
			free(temp);
			return (qd_exec_result){-2};
		}
	}

	// Push temp[0] last (it becomes the top)
	switch (temp[0].type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, temp[0].value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, temp[0].value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, temp[0].value.s);
			free(temp[0].value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, temp[0].value.p);
			break;
		default:
			free(temp);
			return (qd_exec_result){-3};
	}

	free(temp);

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// swap2 - swap top two pairs: ( a b c d -- c d a b )
qd_exec_result qd_swap2(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 4) {
		fprintf(stderr, "Fatal error in swap2: Stack underflow (required 4 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop d, c, b, a
	qd_stack_element_t d, c, b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &d);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &c);
	if (err != QD_STACK_OK) {
		if (d.type == QD_STACK_TYPE_STR) free(d.value.s);
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		if (d.type == QD_STACK_TYPE_STR) free(d.value.s);
		if (c.type == QD_STACK_TYPE_STR) free(c.value.s);
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		if (d.type == QD_STACK_TYPE_STR) free(d.value.s);
		if (c.type == QD_STACK_TYPE_STR) free(c.value.s);
		if (b.type == QD_STACK_TYPE_STR) free(b.value.s);
		return (qd_exec_result){-2};
	}

	// Push in order: c, d, a, b
	// Helper macro to reduce code duplication
	#define PUSH_ELEM(elem, cleanup_on_error) \
		switch (elem.type) { \
			case QD_STACK_TYPE_INT: \
				err = qd_stack_push_int(ctx->st, elem.value.i); \
				break; \
			case QD_STACK_TYPE_FLOAT: \
				err = qd_stack_push_float(ctx->st, elem.value.f); \
				break; \
			case QD_STACK_TYPE_STR: \
				err = qd_stack_push_str(ctx->st, elem.value.s); \
				free(elem.value.s); \
				break; \
			case QD_STACK_TYPE_PTR: \
				err = qd_stack_push_ptr(ctx->st, elem.value.p); \
				break; \
			default: \
				cleanup_on_error \
				return (qd_exec_result){-3}; \
		} \
		if (err != QD_STACK_OK) { \
			cleanup_on_error \
			return (qd_exec_result){-2}; \
		}

	PUSH_ELEM(c, {
		if (d.type == QD_STACK_TYPE_STR) free(d.value.s);
		if (b.type == QD_STACK_TYPE_STR) free(b.value.s);
		if (a.type == QD_STACK_TYPE_STR) free(a.value.s);
	})
	PUSH_ELEM(d, {
		if (b.type == QD_STACK_TYPE_STR) free(b.value.s);
		if (a.type == QD_STACK_TYPE_STR) free(a.value.s);
	})
	PUSH_ELEM(a, {
		if (b.type == QD_STACK_TYPE_STR) free(b.value.s);
	})
	PUSH_ELEM(b, {})

	#undef PUSH_ELEM

	return (qd_exec_result){0};
}

// over2 - copy second pair to top: ( a b c d -- a b c d a b )
qd_exec_result qd_over2(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 4) {
		fprintf(stderr, "Fatal error in over2: Stack underflow (required 4 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the second pair (indices stack_size-4 and stack_size-3)
	qd_stack_element_t elem_a, elem_b;
	qd_stack_error err = qd_stack_element(ctx->st, stack_size - 4, &elem_a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in over2: Failed to access element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	err = qd_stack_element(ctx->st, stack_size - 3, &elem_b);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in over2: Failed to access element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push copies of elem_a and elem_b
	switch (elem_a.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, elem_a.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, elem_a.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, elem_a.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, elem_a.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	switch (elem_b.type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(ctx->st, elem_b.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(ctx->st, elem_b.value.f);
			break;
		case QD_STACK_TYPE_STR:
			err = qd_stack_push_str(ctx->st, elem_b.value.s);
			break;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(ctx->st, elem_b.value.p);
			break;
		default:
			return (qd_exec_result){-3};
	}
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// mod - modulo operation: ( a b -- a%b )
qd_exec_result qd_mod(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in mod: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
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
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_a.type != QD_STACK_TYPE_INT || check_b.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in mod: Type error (expected int for modulo)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	if (b.value.i == 0) {
		fprintf(stderr, "Fatal error in mod: Division by zero\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t result = a.value.i % b.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// neg - negate top element: ( a -- -a )
qd_exec_result qd_neg(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in neg: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_val;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_val);
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in neg: Failed to access stack element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_val.type != QD_STACK_TYPE_INT && check_val.type != QD_STACK_TYPE_FLOAT) {
		fprintf(stderr, "Fatal error in neg: Type error (expected numeric type)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	if (val.type == QD_STACK_TYPE_INT) {
		val.value.i = -val.value.i;
		err = qd_stack_push_int(ctx->st, val.value.i);
	} else {
		val.value.f = -val.value.f;
		err = qd_stack_push_float(ctx->st, val.value.f);
	}

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// min - minimum of top 2 elements: ( a b -- min(a,b) )

// max - maximum of top 2 elements: ( a b -- max(a,b) )

// Threading support
typedef struct {
	qd_context* ctx;
	void* func_ptr;
} qd_thread_info_t;

// Wrapper function for pthread that calls the Quadrate function
static void* qd_thread_wrapper(void* arg) {
	qd_thread_info_t* info = (qd_thread_info_t*)arg;

	// Call the function
	typedef qd_exec_result (*qd_function_ptr)(qd_context*);
	qd_function_ptr func;
	memcpy(&func, &info->func_ptr, sizeof(func));

	if (func) {
		func(info->ctx);
	}

	// Clean up context
	qd_free_context(info->ctx);
	free(info);

	return NULL;
}

// spawn - create a new thread ( fn:ptr -- thread_id:i )
qd_exec_result qd_spawn(qd_context* ctx) {
	// Pop function pointer
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in spawn: Stack underflow\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Verify it's a pointer type
	if (val.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in spawn: Expected pointer type, got %d\n", val.type);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Create new context for the thread
	qd_context* thread_ctx = qd_create_context(1024);
	if (!thread_ctx) {
		fprintf(stderr, "Fatal error in spawn: Failed to create context\n");
		abort();
	}

	// Create thread info
	qd_thread_info_t* info = malloc(sizeof(qd_thread_info_t));
	if (!info) {
		fprintf(stderr, "Fatal error in spawn: Failed to allocate thread info\n");
		qd_free_context(thread_ctx);
		abort();
	}
	info->ctx = thread_ctx;
	info->func_ptr = val.value.p;

	// Create thread
	pthread_t* thread = malloc(sizeof(pthread_t));
	if (!thread) {
		fprintf(stderr, "Fatal error in spawn: Failed to allocate pthread_t\n");
		qd_free_context(thread_ctx);
		free(info);
		abort();
	}

	int result = pthread_create(thread, NULL, qd_thread_wrapper, info);
	if (result != 0) {
		fprintf(stderr, "Fatal error in spawn: pthread_create failed with error %d\n", result);
		qd_free_context(thread_ctx);
		free(info);
		free(thread);
		abort();
	}

	// Push thread ID (as pointer cast to int64_t)
	err = qd_stack_push_int(ctx->st, (int64_t)(uintptr_t)thread);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// detach - detach a thread ( thread_id:i -- )
qd_exec_result qd_detach(qd_context* ctx) {
	// Pop thread ID
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in detach: Stack underflow\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Verify it's an integer type
	if (val.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in detach: Expected integer type, got %d\n", val.type);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get pthread_t pointer
	pthread_t* thread = (pthread_t*)(uintptr_t)val.value.i;

	// Detach thread
	int result = pthread_detach(*thread);
	if (result != 0) {
		fprintf(stderr, "Fatal error in detach: pthread_detach failed with error %d\n", result);
		abort();
	}

	// Free the thread pointer (it's no longer needed after detach)
	free(thread);

	return (qd_exec_result){0};
}

// wait - join/wait for a thread ( thread_id:i -- )
qd_exec_result qd_wait(qd_context* ctx) {
	// Pop thread ID
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in wait: Stack underflow\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Verify it's an integer type
	if (val.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in wait: Expected integer type, got %d\n", val.type);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get pthread_t pointer
	pthread_t* thread = (pthread_t*)(uintptr_t)val.value.i;

	// Join thread
	int result = pthread_join(*thread, NULL);
	if (result != 0) {
		fprintf(stderr, "Fatal error in wait: pthread_join failed with error %d\n", result);
		abort();
	}

	// Free the thread pointer
	free(thread);

	return (qd_exec_result){0};
}

qd_exec_result qd_err(qd_context* ctx) {
	// Check if top of stack is error-tainted and push error code, message, and status
	// Stack before: [value (tainted)]
	// Stack after: [value (untainted), error_msg:str, error_code:int, has_error:int]
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in err: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check that top of stack is tainted
	if (!qd_stack_is_top_tainted(ctx->st)) {
		fprintf(stderr, "Fatal error in err: Top of stack is not error-tainted\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Remove the taint from the top element
	qd_stack_clear_top_taint(ctx->st);

	// Push error message (empty string if no error)
	const char* msg = (ctx->error_code != 0 && ctx->error_msg) ? ctx->error_msg : "";
	qd_stack_error err = qd_stack_push_str(ctx->st, msg);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in err: Failed to push error message\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push error code
	err = qd_stack_push_int(ctx->st, ctx->error_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in err: Failed to push error code\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push has_error flag (0 = no error, non-zero = error)
	int64_t has_error = (ctx->error_code != 0) ? 1 : 0;
	err = qd_stack_push_int(ctx->st, has_error);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in err: Failed to push has_error flag\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Clear error state after checking it
	ctx->error_code = 0;
	if (ctx->error_msg) {
		free(ctx->error_msg);
		ctx->error_msg = NULL;
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_error(qd_context* ctx) {
	// Pop error code and message from stack and set error state
	// Stack before: [error_msg:str, error_code:int]
	// Stack after: []
	qd_stack_element_t error_msg_elem, error_code_elem;

	// Pop error code (integer)
	qd_stack_error err = qd_stack_pop(ctx->st, &error_code_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in error: Stack underflow when popping error code\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (error_code_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in error: Expected integer error code, got type %d\n", error_code_elem.type);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop error message (string)
	err = qd_stack_pop(ctx->st, &error_msg_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in error: Stack underflow when popping error message\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (error_msg_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in error: Expected string error message, got type %d\n", error_msg_elem.type);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		// Free the error code's string if needed
		if (error_code_elem.type == QD_STACK_TYPE_STR) {
			free(error_code_elem.value.s);
		}
		abort();
	}

	// Set error code and message
	ctx->error_code = error_code_elem.value.i;

	// Free old error message if it exists
	if (ctx->error_msg) {
		free(ctx->error_msg);
	}

	// Take ownership of the error message string
	ctx->error_msg = error_msg_elem.value.s;

	return (qd_exec_result){0};
}

// Helper function to check if string is an integer
static bool is_integer(const char* str) {
	if (!str || *str == '\0') {
		return false;
	}

	// Handle optional negative sign
	if (*str == '-') {
		str++;
		if (*str == '\0') {
			return false;
		}
	}

	// Check all remaining chars are digits
	while (*str) {
		if (*str < '0' || *str > '9') {
			return false;
		}
		str++;
	}

	return true;
}

// Helper function to check if string is a float
static bool is_float(const char* str) {
	if (!str || *str == '\0') {
		return false;
	}

	// Handle optional negative sign
	if (*str == '-') {
		str++;
		if (*str == '\0') {
			return false;
		}
	}

	// Must have digits before decimal point
	if (*str < '0' || *str > '9') {
		return false;
	}

	// Skip digits before decimal point
	while (*str >= '0' && *str <= '9') {
		str++;
	}

	// Must have decimal point
	if (*str != '.') {
		return false;
	}
	str++;

	// Must have at least one digit after decimal point
	if (*str < '0' || *str > '9') {
		return false;
	}

	// Check remaining digits
	while (*str) {
		if (*str < '0' || *str > '9') {
			return false;
		}
		str++;
	}

	return true;
}

// Helper function to remove quotes from a string
static char* remove_quotes(const char* str) {
	size_t len = strlen(str);

	// Check if string is quoted
	if (len >= 2 && str[0] == '"' && str[len - 1] == '"') {
		// Allocate new string without quotes
		char* result = (char*)malloc(len - 1);
		if (result) {
			memcpy(result, str + 1, len - 2);
			result[len - 2] = '\0';
		}
		return result;
	}

	// Not quoted, return copy
	return strdup(str);
}

qd_exec_result qd_read(qd_context* ctx) {
	// Read command-line arguments and push onto stack with type inference
	// argv[0] (program name) is saved to ctx->program_name, not pushed to stack
	// Stack before: (empty or anything)
	// Stack after: arg1 arg2 ... argN (argc-1)

	if (ctx->argc == 0 || ctx->argv == NULL) {
		// No arguments, just push 0
		qd_push_i(ctx, 0);
		return (qd_exec_result){0};
	}

	// Save program name (argv[0]) to context
	if (ctx->argc > 0) {
		ctx->program_name = strdup(ctx->argv[0]);
		if (!ctx->program_name) {
			fprintf(stderr, "Fatal error in read: Memory allocation failed for program name\n");
			abort();
		}
	}

	// Push arguments argv[1] onwards onto stack with type inference
	for (int i = 1; i < ctx->argc; i++) {
		const char* arg = ctx->argv[i];

		// Try integer first
		if (is_integer(arg)) {
			int64_t value = atoll(arg);
			qd_push_i(ctx, value);
		}
		// Try float
		else if (is_float(arg)) {
			double value = atof(arg);
			qd_push_f(ctx, value);
		}
		// String (quoted or unquoted)
		else {
			char* str = remove_quotes(arg);
			if (str) {
				qd_push_s(ctx, str);
				free(str);
			} else {
				// Memory allocation failed
				fprintf(stderr, "Fatal error in read: Memory allocation failed\n");
				abort();
			}
		}
	}

	// Finally push argument count (argc - 1, excluding program name)
	qd_push_i(ctx, ctx->argc - 1);

	return (qd_exec_result){0};
}

// Context management functions
qd_context* qd_create_context(size_t stack_size) {
	qd_context* ctx = (qd_context*)malloc(sizeof(qd_context));
	if (ctx) {
		qd_stack_error err = qd_stack_init(&ctx->st, stack_size);
		if (err != QD_STACK_OK) {
			free(ctx);
			return NULL;
		}
		ctx->error_code = 0;
		ctx->error_msg = NULL;
		ctx->argc = 0;
		ctx->argv = NULL;
		ctx->program_name = NULL;
		ctx->call_stack_depth = 0;
	}
	return ctx;
}

void qd_free_context(qd_context* ctx) {
	if (ctx == NULL) {
		return;
	}
	qd_stack_destroy(ctx->st);
	if (ctx->program_name) {
		free(ctx->program_name);
	}
	if (ctx->error_msg) {
		free(ctx->error_msg);
	}
	free(ctx);
}

// Call stack management for debugging/error reporting
void qd_push_call(qd_context* ctx, const char* func_name) {
	if (ctx->call_stack_depth < QD_MAX_CALL_STACK_DEPTH) {
		ctx->call_stack[ctx->call_stack_depth++] = func_name;
	}
}

void qd_pop_call(qd_context* ctx) {
	if (ctx->call_stack_depth > 0) {
		ctx->call_stack_depth--;
	}
}

void qd_print_stack_trace(qd_context* ctx) {
	// Check NO_COLOR environment variable
	const bool use_color = getenv("NO_COLOR") == NULL;
	const char* color_start = use_color ? "\x1b[1;31m" : "";
	const char* color_end = use_color ? "\x1b[0m" : "";

	if (ctx->call_stack_depth == 0) {
		fprintf(stderr, "\n%sStack trace:%s (empty)\n", color_start, color_end);
		return;
	}
	fprintf(stderr, "\n%sStack trace:%s\n", color_start, color_end);
	for (size_t i = ctx->call_stack_depth; i > 0; i--) {
		fprintf(stderr, "  %zu: %s\n", ctx->call_stack_depth - i, ctx->call_stack[i - 1]);
	}
}

void qd_debug_print_stack(qd_context* ctx) {
	if (!ctx || !ctx->st) {
		fprintf(stderr, "Error: Invalid context\n");
		return;
	}

	size_t stack_size = qd_stack_size(ctx->st);

	// Check NO_COLOR environment variable
	const bool use_color = getenv("NO_COLOR") == NULL;
	const char* color_blue = use_color ? "\x1b[1;34m" : "";
	const char* color_green = use_color ? "\x1b[0;32m" : "";
	const char* color_yellow = use_color ? "\x1b[0;33m" : "";
	const char* color_cyan = use_color ? "\x1b[0;36m" : "";
	const char* color_end = use_color ? "\x1b[0m" : "";

	fprintf(stderr, "\n%s=== Data Stack ===%s\n", color_blue, color_end);
	fprintf(stderr, "Size: %zu element%s\n", stack_size, stack_size == 1 ? "" : "s");

	if (stack_size == 0) {
		fprintf(stderr, "(empty)\n");
		return;
	}

	fprintf(stderr, "\nTop of stack (most recent):\n");
	fprintf(stderr, "  %sIdx%s  %sType%s     %sValue%s\n", color_blue, color_end, color_green, color_end, color_yellow, color_end);
	fprintf(stderr, "  ----------------------------------------\n");

	// Print from top (most recent) to bottom
	for (size_t i = stack_size; i > 0; i--) {
		qd_stack_element_t elem;
		qd_stack_error err = qd_stack_element(ctx->st, i - 1, &elem);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "  [%zu]: <error reading element>\n", i - 1);
			continue;
		}

		fprintf(stderr, "  %s[%2zu]%s ", color_blue, i - 1, color_end);

		switch (elem.type) {
		case QD_STACK_TYPE_INT:
			fprintf(stderr, "%sint    %s %s%ld%s\n", color_green, color_end, color_yellow, elem.value.i, color_end);
			break;
		case QD_STACK_TYPE_FLOAT:
			fprintf(stderr, "%sfloat  %s %s%g%s\n", color_green, color_end, color_yellow, elem.value.f, color_end);
			break;
		case QD_STACK_TYPE_STR:
			if (elem.value.s) {
				// Truncate long strings
				if (strlen(elem.value.s) > 40) {
					fprintf(stderr, "%sstring %s %s\"%.37s...\"%s\n", color_green, color_end, color_cyan, elem.value.s, color_end);
				} else {
					fprintf(stderr, "%sstring %s %s\"%s\"%s\n", color_green, color_end, color_cyan, elem.value.s, color_end);
				}
			} else {
				fprintf(stderr, "%sstring %s %s<null>%s\n", color_green, color_end, color_cyan, color_end);
			}
			break;
		case QD_STACK_TYPE_PTR:
			fprintf(stderr, "%sptr    %s %s%p%s\n", color_green, color_end, color_yellow, elem.value.p, color_end);
			break;
		default:
			fprintf(stderr, "<unknown type %d>\n", elem.type);
			break;
		}
	}
	fprintf(stderr, "Bottom of stack (oldest)\n");
	fprintf(stderr, "\n");
}
