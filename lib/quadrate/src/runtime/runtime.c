#include <quadrate/runtime/runtime.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

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
				printf("%s", val.value.s);
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
			printf("flt:%g\n", val.value.f);
			break;
		case QD_STACK_TYPE_STR:
			printf("str:\"%s\"\n", val.value.s);
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
				printf("flt:%g", val.value.f);
				break;
			case QD_STACK_TYPE_STR:
				printf("str:\"%s\"", val.value.s);
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
