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

qd_exec_result qd_div(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in div: Stack underflow (required 2 elements, have %zu)\n", stack_size);
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
		fprintf(stderr, "Fatal error in div: Failed to access stack elements\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in div: Type error (expected numeric types for division)\n");
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
	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		if (b.value.i == 0) {
			return (qd_exec_result){-4};
		}
		int64_t result = a.value.i / b.value.i;
		err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else if ((a.type == QD_STACK_TYPE_INT || a.type == QD_STACK_TYPE_FLOAT) &&
	           (b.type == QD_STACK_TYPE_INT || b.type == QD_STACK_TYPE_FLOAT)) {
		double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
		double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;
		if (bf == 0.0) {
			return (qd_exec_result){-4};
		}
		double result = af / bf;
		err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else {
		if (b.type == QD_STACK_TYPE_STR) {
			free(b.value.s);
		}
		if (a.type == QD_STACK_TYPE_STR) {
			free(a.value.s);
		}
		return (qd_exec_result){-5};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_mul(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in mul: Stack underflow (required 2 elements, have %zu)\n", stack_size);
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
		fprintf(stderr, "Fatal error in mul: Failed to access stack elements\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in mul: Type error (expected numeric types for multiplication)\n");
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
	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		int64_t result = a.value.i * b.value.i;
		err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else if ((a.type == QD_STACK_TYPE_INT || a.type == QD_STACK_TYPE_FLOAT) &&
	           (b.type == QD_STACK_TYPE_INT || b.type == QD_STACK_TYPE_FLOAT)) {
		double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
		double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;
		double result = af * bf;
		err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else {
		if (b.type == QD_STACK_TYPE_STR) {
			free(b.value.s);
		}
		if (a.type == QD_STACK_TYPE_STR) {
			free(a.value.s);
		}
		return (qd_exec_result){-5};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_add(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in add: Stack underflow (required 2 elements, have %zu)\n", stack_size);
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
		fprintf(stderr, "Fatal error in add: Failed to access stack elements\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in add: Type error (expected numeric types for addition)\n");
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
	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		int64_t result = a.value.i + b.value.i;
		err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else if ((a.type == QD_STACK_TYPE_INT || a.type == QD_STACK_TYPE_FLOAT) &&
	           (b.type == QD_STACK_TYPE_INT || b.type == QD_STACK_TYPE_FLOAT)) {
		double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
		double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;
		double result = af + bf;
		err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else {
		if (b.type == QD_STACK_TYPE_STR) {
			free(b.value.s);
		}
		if (a.type == QD_STACK_TYPE_STR) {
			free(a.value.s);
		}
		return (qd_exec_result){-5};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_sub(qd_context* ctx) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in sub: Stack underflow (required 2 elements, have %zu)\n", stack_size);
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
		fprintf(stderr, "Fatal error in sub: Failed to access stack elements\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in sub: Type error (expected numeric types for subtraction)\n");
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
	if (a.type == QD_STACK_TYPE_INT && b.type == QD_STACK_TYPE_INT) {
		int64_t result = a.value.i - b.value.i;
		err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else if ((a.type == QD_STACK_TYPE_INT || a.type == QD_STACK_TYPE_FLOAT) &&
	           (b.type == QD_STACK_TYPE_INT || b.type == QD_STACK_TYPE_FLOAT)) {
		double af = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
		double bf = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;
		double result = af - bf;
		err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else {
		if (b.type == QD_STACK_TYPE_STR) {
			free(b.value.s);
		}
		if (a.type == QD_STACK_TYPE_STR) {
			free(a.value.s);
		}
		return (qd_exec_result){-5};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_sq(qd_context* ctx) {
	// Check we have at least 1 element
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in sq: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check it's a numeric type (int or float)
	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in sq: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in sq: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	if (a.type == QD_STACK_TYPE_INT) {
		int64_t result = a.value.i * a.value.i;
		err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else if (a.type == QD_STACK_TYPE_FLOAT) {
		double result = a.value.f * a.value.f;
		err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else {
		return (qd_exec_result){-5};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_abs(qd_context* ctx) {
	// Check we have at least 1 element
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in abs: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check it's a numeric type (int or float)
	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in abs: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in abs: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	if (a.type == QD_STACK_TYPE_INT) {
		int64_t result = a.value.i < 0 ? -a.value.i : a.value.i;
		err = qd_stack_push_int(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else if (a.type == QD_STACK_TYPE_FLOAT) {
		double result = a.value.f < 0.0 ? -a.value.f : a.value.f;
		err = qd_stack_push_float(ctx->st, result);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
	} else {
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

qd_exec_result qd_sin(qd_context* ctx) {
	// Compute sine of the top value (in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in sin: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check it's a numeric type (int or float)
	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in sin: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in sin: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double result = sin(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_cos(qd_context* ctx) {
	// Compute cosine of the top value (in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in cos: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in cos: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in cos: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double result = cos(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_tan(qd_context* ctx) {
	// Compute tangent of the top value (in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in tan: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in tan: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in tan: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double result = tan(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_asin(qd_context* ctx) {
	// Compute arcsine of the top value (result in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in asin: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in asin: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in asin: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;

	// Check domain: asin requires value in [-1, 1]
	if (value < -1.0 || value > 1.0) {
		fprintf(stderr, "Fatal error in asin: Domain error (value %f is outside [-1, 1])\n", value);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = asin(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_acos(qd_context* ctx) {
	// Compute arccosine of the top value (result in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in acos: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in acos: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in acos: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;

	// Check domain: acos requires value in [-1, 1]
	if (value < -1.0 || value > 1.0) {
		fprintf(stderr, "Fatal error in acos: Domain error (value %f is outside [-1, 1])\n", value);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = acos(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_atan(qd_context* ctx) {
	// Compute arctangent of the top value (result in radians)
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in atan: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in atan: Failed to peek stack\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in atan: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	double value = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double result = atan(value);
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// sqrt - square root
qd_exec_result qd_sqrt(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in sqrt: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in sqrt: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in sqrt: Invalid type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check domain: sqrt requires non-negative values
	if (value < 0.0) {
		fprintf(stderr, "Fatal error in sqrt: Domain error (requires non-negative value, got %f)\n", value);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = sqrt(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// cb - cube (x^3)
qd_exec_result qd_cb(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in cb: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in cb: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in cb: Invalid type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = value * value * value;

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// cbrt - cube root
qd_exec_result qd_cbrt(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in cbrt: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in cbrt: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in cbrt: Invalid type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = cbrt(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// ceil - ceiling (round up)
qd_exec_result qd_ceil(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in ceil: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in ceil: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in ceil: Invalid type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = ceil(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// floor - floor (round down)
qd_exec_result qd_floor(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in floor: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in floor: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in floor: Invalid type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = floor(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// ln - natural logarithm (base e)
qd_exec_result qd_ln(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in ln: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in ln: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in ln: Invalid type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check domain: ln requires positive values
	if (value <= 0.0) {
		fprintf(stderr, "Fatal error in ln: Domain error (requires positive value, got %f)\n", value);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = log(value);  // log() is natural logarithm in C

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// log10 - base 10 logarithm
qd_exec_result qd_log10(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in log10: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in log10: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in log10: Invalid type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check domain: log10 requires positive values
	if (value <= 0.0) {
		fprintf(stderr, "Fatal error in log10: Domain error (requires positive value, got %f)\n", value);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double result = log10(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

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

// pow - exponentiation (base^exponent)
qd_exec_result qd_pow(qd_context* ctx) {
	// Pop two numeric values: base, then exponent
	// Push result as float
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in pow: Stack underflow (requires 2 values)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop exponent (top of stack)
	qd_stack_element_t exponent_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &exponent_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in pow: Failed to pop exponent\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop base
	qd_stack_element_t base_elem;
	err = qd_stack_pop(ctx->st, &base_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in pow: Failed to pop base\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Convert both to double
	double base, exponent;
	if (base_elem.type == QD_STACK_TYPE_INT) {
		base = (double)base_elem.value.i;
	} else if (base_elem.type == QD_STACK_TYPE_FLOAT) {
		base = base_elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in pow: Invalid base type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (exponent_elem.type == QD_STACK_TYPE_INT) {
		exponent = (double)exponent_elem.value.i;
	} else if (exponent_elem.type == QD_STACK_TYPE_FLOAT) {
		exponent = exponent_elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in pow: Invalid exponent type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Compute base^exponent
	double result = pow(base, exponent);

	// Push result as float
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// round - round to nearest integer
qd_exec_result qd_round(qd_context* ctx) {
	// Pop one numeric value, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in round: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in round: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in round: Invalid type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Round to nearest integer
	double result = round(value);

	// Push result as float
	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

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
qd_exec_result qd_fac(qd_context* ctx) {
	// Pop one integer value, compute factorial, push integer result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in fac: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in fac: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in fac: Invalid type (expected int)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t n = elem.value.i;

	// Check for negative numbers
	if (n < 0) {
		fprintf(stderr, "Fatal error in fac: Factorial of negative number (%ld)\n", n);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Compute factorial
	int64_t result = 1;
	for (int64_t i = 2; i <= n; i++) {
		// Check for overflow
		if (result > INT64_MAX / i) {
			fprintf(stderr, "Fatal error in fac: Factorial overflow for %ld\n", n);
			dump_stack(ctx);
			abort();
		}
		result *= i;
	}

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// inv - inverse/reciprocal (1/x, returns float)
qd_exec_result qd_inv(qd_context* ctx) {
	// Pop one numeric value, compute 1/x, push float result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in inv: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in inv: Failed to pop value\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	double value;
	if (elem.type == QD_STACK_TYPE_INT) {
		value = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		value = elem.value.f;
	} else {
		fprintf(stderr, "Fatal error in inv: Invalid type (expected int or float)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check for division by zero
	if (value == 0.0) {
		fprintf(stderr, "Fatal error in inv: Division by zero\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Compute 1/x as float
	double result = 1.0 / value;

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

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
qd_exec_result qd_min(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in min: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in min: Failed to access stack elements\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in min: Type error (expected numeric types)\n");
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

	// Convert to common type and compare
	double a_val = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double b_val = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;

	double min_val = (a_val <= b_val) ? a_val : b_val;

	// If either operand is float, result is float (type promotion)
	if (a.type == QD_STACK_TYPE_FLOAT || b.type == QD_STACK_TYPE_FLOAT) {
		err = qd_stack_push_float(ctx->st, min_val);
	} else {
		// Both are INT
		err = qd_stack_push_int(ctx->st, (int64_t)min_val);
	}

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// max - maximum of top 2 elements: ( a b -- max(a,b) )
qd_exec_result qd_max(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in max: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in max: Failed to access stack elements\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in max: Type error (expected numeric types)\n");
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

	// Convert to common type and compare
	double a_val = (a.type == QD_STACK_TYPE_INT) ? (double)a.value.i : a.value.f;
	double b_val = (b.type == QD_STACK_TYPE_INT) ? (double)b.value.i : b.value.f;

	double max_val = (a_val >= b_val) ? a_val : b_val;

	// If either operand is float, result is float (type promotion)
	if (a.type == QD_STACK_TYPE_FLOAT || b.type == QD_STACK_TYPE_FLOAT) {
		err = qd_stack_push_float(ctx->st, max_val);
	} else {
		// Both are INT
		err = qd_stack_push_int(ctx->st, (int64_t)max_val);
	}

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// and - bitwise/logical AND: ( a b -- a&b )
qd_exec_result qd_and(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in and: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in and: Failed to access stack elements\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_a.type != QD_STACK_TYPE_INT || check_b.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in and: Type error (expected int for bitwise operation)\n");
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

	int64_t result = a.value.i & b.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// or - bitwise/logical OR: ( a b -- a|b )
qd_exec_result qd_or(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in or: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in or: Failed to access stack elements\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_a.type != QD_STACK_TYPE_INT || check_b.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in or: Type error (expected int for bitwise operation)\n");
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

	int64_t result = a.value.i | b.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// not - bitwise/logical NOT: ( a -- ~a )
qd_exec_result qd_not(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in not: Stack underflow (required 1 element, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_val;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_val);
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in not: Failed to access stack element\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_val.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in not: Type error (expected int for bitwise operation)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	int64_t result = ~val.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// xor - bitwise XOR: ( a b -- a^b )
qd_exec_result qd_xor(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in xor: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in xor: Failed to access stack elements\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_a.type != QD_STACK_TYPE_INT || check_b.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in xor: Type error (expected int for bitwise operation)\n");
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

	int64_t result = a.value.i ^ b.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// lshift - logical shift left: ( x n -- x<<n )
qd_exec_result qd_lshift(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in lshift: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_n, check_x;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_n);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_x);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in lshift: Failed to access stack elements\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_x.type != QD_STACK_TYPE_INT || check_n.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in lshift: Type error (expected int for bitwise operation)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t n, x;
	qd_stack_error err = qd_stack_pop(ctx->st, &n);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &x);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Check for negative or excessive shift counts
	if (n.value.i < 0 || n.value.i >= 64) {
		fprintf(stderr, "Fatal error in lshift: Shift count out of range (must be 0-63, got %ld)\n", n.value.i);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t result = x.value.i << n.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// rshift - logical shift right: ( x n -- x>>n )
qd_exec_result qd_rshift(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in rshift: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_n, check_x;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_n);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_x);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in rshift: Failed to access stack elements\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_x.type != QD_STACK_TYPE_INT || check_n.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in rshift: Type error (expected int for bitwise operation)\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t n, x;
	qd_stack_error err = qd_stack_pop(ctx->st, &n);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &x);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Check for negative or excessive shift counts
	if (n.value.i < 0 || n.value.i >= 64) {
		fprintf(stderr, "Fatal error in rshift: Shift count out of range (must be 0-63, got %ld)\n", n.value.i);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Logical shift right (unsigned)
	int64_t result = (int64_t)((uint64_t)x.value.i >> n.value.i);

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

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
	// Check if top of stack is error-tainted and push the error status
	// Stack before: [value (tainted)]
	// Stack after: [value (untainted), error_status]
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

	// Push error status (0 = no error, 1 = error)
	int64_t error_status = ctx->has_error ? 1 : 0;
	qd_stack_error err = qd_stack_push_int(ctx->st, error_status);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in err: Failed to push error status\n");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Clear error flag after checking it
	ctx->has_error = false;

	return (qd_exec_result){0};
}

qd_exec_result qd_error(qd_context* ctx) {
	// Set the error flag
	// Stack before: [...anything...]
	// Stack after: [...anything...] (unchanged)
	ctx->has_error = true;
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
		ctx->has_error = false;
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
