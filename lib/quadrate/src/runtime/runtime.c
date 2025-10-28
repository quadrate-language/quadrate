#include <quadrate/runtime/runtime.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

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

qd_exec_result qd_print(qd_context* ctx) {
	// Forth-style: pop and print the top element only
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
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
			printf("\"%s\"\n", val.value.s);
			free(val.value.s);  // Free the string memory after printing
			break;
		default:
			return (qd_exec_result){-3};
	}

	return (qd_exec_result){0};
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
				printf("\"%s\"", val.value.s);
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
			printf("string:\"%s\"\n", val.value.s);
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
				printf("string:\"%s\"", val.value.s);
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
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in div: Type error (expected numeric types for division)\n");
		dump_stack(ctx);
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
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in mul: Type error (expected numeric types for multiplication)\n");
		dump_stack(ctx);
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
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in add: Type error (expected numeric types for addition)\n");
		dump_stack(ctx);
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
		abort();
	}

	if ((check_a.type != QD_STACK_TYPE_INT && check_a.type != QD_STACK_TYPE_FLOAT) ||
	    (check_b.type != QD_STACK_TYPE_INT && check_b.type != QD_STACK_TYPE_FLOAT)) {
		fprintf(stderr, "Fatal error in sub: Type error (expected numeric types for subtraction)\n");
		dump_stack(ctx);
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
		abort();
	}

	// Check it's a numeric type (int or float)
	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in sq: Failed to peek stack\n");
		dump_stack(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in sq: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
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
		abort();
	}

	// Check it's a numeric type (int or float)
	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in abs: Failed to peek stack\n");
		dump_stack(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in abs: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
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
		abort();
	}

	qd_stack_element_t top;
	qd_stack_error err = qd_stack_peek(ctx->st, &top);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dup: Failed to peek stack\n");
		dump_stack(ctx);
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

qd_exec_result qd_swap(qd_context* ctx) {
	// Swap the top two elements of the stack
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in swap: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		dump_stack(ctx);
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
		abort();
	}

	// Get the second element (without popping)
	qd_stack_element_t second;
	qd_stack_error err = qd_stack_element(ctx->st, stack_size - 2, &second);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in over: Failed to access second element\n");
		dump_stack(ctx);
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
		abort();
	}

	// Check it's a numeric type (int or float)
	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in sin: Failed to peek stack\n");
		dump_stack(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in sin: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
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
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in cos: Failed to peek stack\n");
		dump_stack(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in cos: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
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
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in tan: Failed to peek stack\n");
		dump_stack(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in tan: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
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
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in asin: Failed to peek stack\n");
		dump_stack(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in asin: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
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
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in acos: Failed to peek stack\n");
		dump_stack(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in acos: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
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
		abort();
	}

	qd_stack_element_t a;
	qd_stack_error err = qd_stack_peek(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in atan: Failed to peek stack\n");
		dump_stack(ctx);
		abort();
	}
	if (a.type != QD_STACK_TYPE_INT && a.type != QD_STACK_TYPE_FLOAT) {
		const char* type_name = "unknown";
		if (a.type == QD_STACK_TYPE_STR) type_name = "str";
		else if (a.type == QD_STACK_TYPE_PTR) type_name = "ptr";
		fprintf(stderr, "Fatal error in atan: Type error (expected int or float, got %s)\n", type_name);
		dump_stack(ctx);
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
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in sqrt: Failed to pop value\n");
		dump_stack(ctx);
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
		abort();
	}

	// Check domain: sqrt requires non-negative values
	if (value < 0.0) {
		fprintf(stderr, "Fatal error in sqrt: Domain error (requires non-negative value, got %f)\n", value);
		dump_stack(ctx);
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
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in cb: Failed to pop value\n");
		dump_stack(ctx);
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
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in cbrt: Failed to pop value\n");
		dump_stack(ctx);
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
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in ceil: Failed to pop value\n");
		dump_stack(ctx);
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
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in floor: Failed to pop value\n");
		dump_stack(ctx);
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
		abort();
	}

	double result = floor(value);

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// dec - decrement (subtract 1, preserves type)
qd_exec_result qd_dec(qd_context* ctx) {
	// Pop one numeric value, subtract 1, push result with same type
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in dec: Stack underflow (requires 1 value)\n");
		dump_stack(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dec: Failed to pop value\n");
		dump_stack(ctx);
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
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in inc: Failed to pop value\n");
		dump_stack(ctx);
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
		abort();
	}

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
			abort();
		}
	}
}
