#define _DEFAULT_SOURCE
// I/O operations for Quadrate runtime
// Split from runtime.c for maintainability

#define _POSIX_C_SOURCE 200809L

#include "runtime_internal.h"
#include <ctype.h>
#include <inttypes.h>
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

/* Renders a pointer the way `print` and `dump` should: an array as its elements, anything else
 * as what it is. A pointer used to print as nothing at all -- `[1 2 3] print` produced an empty
 * line -- which made arrays and structs the one kind of value you could not look at while
 * debugging.
 *
 * The runtime can tell an array from a struct by the magic word each carries, but a struct header
 * holds no field names, so a struct can only be reported as one. Depth is capped because an array
 * may contain itself. */
#define QD_PRINT_MAX_DEPTH 8

static void print_pointer(const void* p, uint8_t kind, int depth);

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
			printf("%" PRId64, v);
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
		case QD_ARRAY_TYPE_D128: {
			qd_d128_t v = qd_d128_from_i64(0);
			qd_array_get_d128(arr, i, &v);
			char text[QD_D128_STRING_MAX];
			qd_d128_format(v, text, sizeof text);
			printf("%s", text);
			break;
		}
		default: {
			void* v = NULL;
			qd_array_get_ptr(arr, i, &v);
			print_pointer(v, arr->elemKind, depth + 1);
			break;
		}
		}
	}
	printf("]");
}

// A pointer is shown as what its kind says it is
static void print_pointer(const void* p, uint8_t kind, int depth) {
	if (!p) {
		printf("null");
		return;
	}
	if (kind == QD_PTR_ARRAY) {
		if (depth >= QD_PRINT_MAX_DEPTH) {
			printf("[...]");
			return;
		}
		print_array_contents((const qd_array_t*)p, depth);
		return;
	}
	if (kind == QD_PTR_STRUCT) {
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
		printf("%" PRId64, val.value.i);
		break;
	case QD_STACK_TYPE_FLOAT:
		printf("%g", val.value.f);
		break;
	case QD_STACK_TYPE_STR:
		printf("%s", qd_string_data(val.value.s));
		qd_string_release(val.value.s); // Release the string reference after printing
		break;
	case QD_STACK_TYPE_PTR:
		print_pointer(val.value.p, val.ptr_kind, 0);
		qdrt_release_element(&val); // The stack held a reference; printing consumes it
		break;
	case QD_STACK_TYPE_D128: {
		char text[QD_D128_STRING_MAX];
		qd_d128_format(val.value.d, text, sizeof text);
		printf("%s", text);
		break;
	}
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

int qd_dump(qd_context* ctx) {
	// Forth-style verbose: pop and print the top element with type info
	QDRT_CHECK_STACK(ctx, "dump", 1);

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "dump", "Failed to pop value");
	}

	switch (val.type) {
	case QD_STACK_TYPE_INT:
		printf("int:%" PRId64 "\n", val.value.i);
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
		print_pointer(val.value.p, val.ptr_kind, 0);
		printf("\n");
		qdrt_release_element(&val);
		break;
	case QD_STACK_TYPE_D128: {
		char text[QD_D128_STRING_MAX];
		qd_d128_format(val.value.d, text, sizeof text);
		printf("d128:%s\n", text);
		break;
	}
	default:
		return (int){-3};
	}

	return (int){0};
}
