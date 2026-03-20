#define _DEFAULT_SOURCE
// I/O operations for Quadrate runtime
// Split from runtime.c for maintainability

#define _POSIX_C_SOURCE 200809L

#include "runtime_internal.h"
#include <ctype.h>
#include <quadrate/rt/qd_string.h>
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

int qd_print(qd_context* ctx) {
	// Pop and print the top element
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (int){-2};
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
			return (int){-2};
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
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (int){-2};
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
		printf("ptr:%p\n", val.value.p);
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
			return (int){-2};
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

int qd_read(qd_context* ctx) {
	// Read command-line arguments and push onto stack with type inference
	// argv[0] (program name) is saved to ctx->program_name, not pushed to stack
	// Stack before: (empty or anything)
	// Stack after: arg1 arg2 ... argN (argc-1)

	if (ctx->argc == 0 || ctx->argv == NULL) {
		// No arguments, just push 0
		qd_push_i(ctx, 0);
		return (int){0};
	}

	// Save program name (argv[0]) to context
	if (ctx->argc > 0) {
		free(ctx->program_name);
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

	return (int){0};
}
