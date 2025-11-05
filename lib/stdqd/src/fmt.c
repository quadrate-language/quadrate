#include <stdqd/fmt.h>
#include <runtime/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to count format specifiers in format string
static int count_format_specifiers(const char* fmt) {
	int count = 0;
	for (const char* p = fmt; *p; p++) {
		if (*p == '%') {
			p++; // Skip '%'
			if (*p == '%') {
				// Literal '%', not a format specifier
				continue;
			} else if (*p == 's' || *p == 'd' || *p == 'i' || *p == 'f') {
				count++;
			}
		}
	}
	return count;
}

qd_exec_result qd_stdqd_printf(qd_context* ctx) {
	// Pop format string from stack
	qd_stack_element_t fmt_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &fmt_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in qd_stdqd_printf: Failed to pop format string\n");
		abort();
	}

	if (fmt_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in qd_stdqd_printf: Expected string format, got type %d\n", fmt_elem.type);
		free(fmt_elem.value.s); // Clean up if it was a string
		abort();
	}

	const char* format = fmt_elem.value.s;

	// Count how many arguments we need
	int arg_count = count_format_specifiers(format);

	// Check stack has enough arguments
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < (size_t)arg_count) {
		fprintf(stderr, "Fatal error in qd_stdqd_printf: Format string requires %d arguments, but stack has %zu\n",
			arg_count, stack_size);
		free(fmt_elem.value.s);
		abort();
	}

	// Pop all arguments into temporary array (in reverse order)
	qd_stack_element_t* args = malloc(sizeof(qd_stack_element_t) * (size_t)arg_count);
	if (!args && arg_count > 0) {
		fprintf(stderr, "Fatal error in qd_stdqd_printf: Memory allocation failed\n");
		free(fmt_elem.value.s);
		abort();
	}

	for (int i = arg_count - 1; i >= 0; i--) {
		err = qd_stack_pop(ctx->st, &args[i]);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "Fatal error in qd_stdqd_printf: Failed to pop argument %d\n", i);
			free(args);
			free(fmt_elem.value.s);
			abort();
		}
	}

	// Process format string and print
	int arg_idx = 0;
	for (const char* p = format; *p; p++) {
		if (*p == '%' && *(p + 1)) {
			p++; // Skip '%'

			if (*p == '%') {
				// Literal '%'
				putchar('%');
			} else if (*p == 's') {
				// String argument
				if (arg_idx >= arg_count) {
					fprintf(stderr, "Fatal error in qd_stdqd_printf: Not enough arguments for format string\n");
					free(args);
					free(fmt_elem.value.s);
					abort();
				}
				if (args[arg_idx].type != QD_STACK_TYPE_STR) {
					fprintf(stderr, "Fatal error in qd_stdqd_printf: Expected string for %%s, got type %d\n",
						args[arg_idx].type);
					free(args);
					free(fmt_elem.value.s);
					abort();
				}
				printf("%s", args[arg_idx].value.s);
				arg_idx++;
			} else if (*p == 'd' || *p == 'i') {
				// Integer argument
				if (arg_idx >= arg_count) {
					fprintf(stderr, "Fatal error in qd_stdqd_printf: Not enough arguments for format string\n");
					free(args);
					free(fmt_elem.value.s);
					abort();
				}
				if (args[arg_idx].type != QD_STACK_TYPE_INT) {
					fprintf(stderr, "Fatal error in qd_stdqd_printf: Expected int for %%d, got type %d\n",
						args[arg_idx].type);
					free(args);
					free(fmt_elem.value.s);
					abort();
				}
				printf("%ld", args[arg_idx].value.i);
				arg_idx++;
			} else if (*p == 'f') {
				// Float argument
				if (arg_idx >= arg_count) {
					fprintf(stderr, "Fatal error in qd_stdqd_printf: Not enough arguments for format string\n");
					free(args);
					free(fmt_elem.value.s);
					abort();
				}
				if (args[arg_idx].type != QD_STACK_TYPE_FLOAT) {
					fprintf(stderr, "Fatal error in qd_stdqd_printf: Expected float for %%f, got type %d\n",
						args[arg_idx].type);
					free(args);
					free(fmt_elem.value.s);
					abort();
				}
				printf("%f", args[arg_idx].value.f);
				arg_idx++;
			} else {
				// Unknown format specifier, just print it
				putchar('%');
				putchar(*p);
			}
		} else {
			// Regular character
			putchar(*p);
		}
	}

	// Clean up
	for (int i = 0; i < arg_count; i++) {
		if (args[i].type == QD_STACK_TYPE_STR) {
			free(args[i].value.s);
		}
	}
	free(args);
	free(fmt_elem.value.s);

	return (qd_exec_result){0};
}
