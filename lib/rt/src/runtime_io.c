#define _DEFAULT_SOURCE
// I/O operations for Quadrate runtime
// Split from runtime.c for maintainability

#define _POSIX_C_SOURCE 200809L

#include "runtime_internal.h"
#include <ctype.h>
#include <quadrate/rt/array.h>
#include <quadrate/rt/qd_string.h>
#include <quadrate/rt/qd_struct.h>
#include <quadrate/rt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to check if string contains whitespace
static bool has_whitespace(const char* str) {
	while (*str) {
		if (isspace((unsigned char)*str)) {
			return true;
		}
		str++;
	}
	return false;
}

/* Renders a pointer the way `print` and `printv` should: an array as its elements, anything else
 * as what it is. A pointer used to print as nothing at all -- `[1 2 3] print` produced an empty
 * line -- which made arrays and structs the one kind of value you could not look at while
 * debugging.
 *
 * The runtime can tell an array from a struct by the magic word each carries, but a struct header
 * holds no field names, so a struct can only be reported as one. Depth is capped because an array
 * may contain itself. */
#define QD_PRINT_MAX_DEPTH 8

static void print_pointer(const void* p, int depth);

static void print_array_contents(const qd_array_t* arr, int depth) {
	const size_t n = qd_array_length(arr);
	printf("[");
	for (size_t i = 0; i < n; i++) {
		if (i > 0) {
			printf(" ");
		}
		switch (arr->elemType) {
		case QD_ARRAY_TYPE_INT: {
			int64_t v = 0;
			qd_array_get_int(arr, i, &v);
			printf("%ld", v);
			break;
		}
		case QD_ARRAY_TYPE_FLOAT: {
			double v = 0;
			qd_array_get_float(arr, i, &v);
			printf("%g", v);
			break;
		}
		case QD_ARRAY_TYPE_STR: {
			void* v = NULL;
			qd_array_get_ptr(arr, i, &v);
			printf("%s", v ? qd_string_data((qd_string_t*)v) : "");
			break;
		}
		default: {
			void* v = NULL;
			qd_array_get_ptr(arr, i, &v);
			print_pointer(v, depth + 1);
			break;
		}
		}
	}
	printf("]");
}

static void print_pointer(const void* p, int depth) {
	if (!p) {
		printf("null");
		return;
	}
	if (qd_array_is_valid(p)) {
		if (depth >= QD_PRINT_MAX_DEPTH) {
			printf("[...]");
			return;
		}
		print_array_contents((const qd_array_t*)p, depth);
		return;
	}
	if (qd_struct_is_valid(p)) {
		/* The header carries a refcount and a destructor, not field names. */
		printf("<struct %p>", p);
		return;
	}
	printf("<ptr %p>", p);
}

int qd_print(qd_context* ctx) {
	// Pop and print the top element
	QDRT_CHECK_STACK(ctx, "print", 1);

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "print", "Failed to pop value");
	}

	switch (val.type) {
	case QD_STACK_TYPE_INT:
		printf("%ld", val.value.i);
		break;
	case QD_STACK_TYPE_FLOAT:
		printf("%g", val.value.f);
		break;
	case QD_STACK_TYPE_STR:
		printf("%s", qd_string_data(val.value.s));
		qd_string_release(val.value.s); // Release the string reference after printing
		break;
	case QD_STACK_TYPE_PTR:
		print_pointer(val.value.p, 0);
		qd_ptr_release(val.value.p); // The stack held a reference; printing consumes it
		break;
	default:
		return (int){-3};
	}

	return (int){0};
}

int qd_nl(qd_context* ctx) {
	(void)ctx; // Unused parameter
	printf("\n");
	return (int){0};
}

int qd_prints(qd_context* ctx) {
	// Print entire stack (non-destructive) - output only values for piping
	const size_t stack_size = qd_stack_size(ctx->st);

	// Print from bottom to top, all on one line
	for (size_t i = 0; i < stack_size; i++) {
		qd_stack_element_t val;
		qd_stack_error err = qd_stack_element(ctx->st, i, &val);
		if (err != QD_STACK_OK) {
			QDRT_FATAL(ctx, "prints", "Failed to read stack element %zu", i);
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
		case QD_STACK_TYPE_STR: {
			const char* str_data = qd_string_data(val.value.s);
			// Smart quoting: only quote if string contains whitespace
			if (has_whitespace(str_data)) {
				printf("\"%s\"", str_data);
			} else {
				printf("%s", str_data);
			}
			break;
		}
		default:
			return (int){-3};
		}
	}

	if (stack_size > 0) {
		printf("\n");
	}

	return (int){0};
}

int qd_printv(qd_context* ctx) {
	// Forth-style verbose: pop and print the top element with type info
	QDRT_CHECK_STACK(ctx, "printv", 1);

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "printv", "Failed to pop value");
	}

	switch (val.type) {
	case QD_STACK_TYPE_INT:
		printf("int:%ld\n", val.value.i);
		break;
	case QD_STACK_TYPE_FLOAT:
		printf("float:%g\n", val.value.f);
		break;
	case QD_STACK_TYPE_STR: {
		const char* str_data = qd_string_data(val.value.s);
		// Smart quoting: only quote if string contains whitespace
		if (has_whitespace(str_data)) {
			printf("string:\"%s\"\n", str_data);
		} else {
			printf("string:%s\n", str_data);
		}
		qd_string_release(val.value.s); // Release the string reference after printing
		break;
	}
	case QD_STACK_TYPE_PTR:
		printf("ptr:");
		print_pointer(val.value.p, 0);
		printf("\n");
		qd_ptr_release(val.value.p);
		break;
	default:
		return (int){-3};
	}

	return (int){0};
}

int qd_printsv(qd_context* ctx) {
	// Print entire stack with type info (non-destructive)
	const size_t stack_size = qd_stack_size(ctx->st);

	// Print from bottom to top, all on one line
	for (size_t i = 0; i < stack_size; i++) {
		qd_stack_element_t val;
		qd_stack_error err = qd_stack_element(ctx->st, i, &val);
		if (err != QD_STACK_OK) {
			QDRT_FATAL(ctx, "printsv", "Failed to read stack element %zu", i);
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
		case QD_STACK_TYPE_STR: {
			const char* str_data = qd_string_data(val.value.s);
			// Smart quoting: only quote if string contains whitespace
			if (has_whitespace(str_data)) {
				printf("string:\"%s\"", str_data);
			} else {
				printf("string:%s", str_data);
			}
			break;
		}
		case QD_STACK_TYPE_PTR:
			printf("ptr:%p", val.value.p);
			break;
		default:
			return (int){-3};
		}
	}

	if (stack_size > 0) {
		printf("\n");
	}

	return (int){0};
}

