#include <stdqd/fmt.h>
#include <qdrt/stack.h>
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

qd_exec_result usr_fmt_printf(qd_context* ctx) {
	// Stack order: ( format:s arg1 arg2 ... argN -- )
	// Format string is at the bottom, arguments are on top

	// First, we need to collect all arguments and find the format string
	// We'll pop everything into a temporary array and find the bottommost string
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size == 0) {
		fprintf(stderr, "Fatal error in usr_fmt_printf: Stack underflow\n");
		abort();
	}

	// Pop all elements from stack
	qd_stack_element_t* elements = malloc(sizeof(qd_stack_element_t) * stack_size);
	if (!elements) {
		fprintf(stderr, "Fatal error in usr_fmt_printf: Memory allocation failed\n");
		abort();
	}

	for (size_t i = 0; i < stack_size; i++) {
		qd_stack_error err = qd_stack_pop(ctx->st, &elements[i]);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "Fatal error in usr_fmt_printf: Failed to pop element\n");
			free(elements);
			abort();
		}
	}

	// Find the bottommost string on the stack - that's the format string
	// (There may be other values below it from control flow constructs)
	int fmt_idx = -1;
	for (int i = (int)stack_size - 1; i >= 0; i--) {
		if (elements[i].type == QD_STACK_TYPE_STR) {
			fmt_idx = i;
			break; // Found the bottommost string
		}
	}

	if (fmt_idx == -1) {
		fprintf(stderr, "Fatal error in usr_fmt_printf: No format string found on stack\n");
		for (size_t i = 0; i < stack_size; i++) {
			if (elements[i].type == QD_STACK_TYPE_STR) free(elements[i].value.s);
		}
		free(elements);
		abort();
	}

	qd_stack_element_t fmt_elem = elements[fmt_idx];

	const char* format = fmt_elem.value.s;
	int arg_count = count_format_specifiers(format);

	// Arguments are the elements between format (fmt_idx) and top of stack (index 0)
	// Available arguments: fmt_idx elements above format
	if (fmt_idx < arg_count) {
		fprintf(stderr, "Fatal error in usr_fmt_printf: Format requires %d arguments, but got %d\n",
			arg_count, fmt_idx);
		for (size_t i = 0; i < stack_size; i++) {
			if (elements[i].type == QD_STACK_TYPE_STR) free(elements[i].value.s);
		}
		free(elements);
		abort();
	}

	// Arguments are in reverse order: elements[arg_count-1] is first arg, elements[0] is last arg
	// Process format string and print
	int arg_idx = arg_count - 1; // Start from the last argument
	for (const char* p = format; *p; p++) {
		if (*p == '%' && *(p + 1)) {
			p++; // Skip '%'

			if (*p == '%') {
				// Literal '%'
				putchar('%');
			} else if (*p == 's') {
				// String argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Not enough arguments for format string\n");
					free(elements);
					free(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_STR) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Expected string for %%s, got type %d\n",
						elements[arg_idx].type);
					free(elements);
					free(fmt_elem.value.s);
					abort();
				}
				printf("%s", elements[arg_idx].value.s);
				arg_idx--;
			} else if (*p == 'd' || *p == 'i') {
				// Integer argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Not enough arguments for format string\n");
					free(elements);
					free(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_INT) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Expected int for %%d, got type %d\n",
						elements[arg_idx].type);
					free(elements);
					free(fmt_elem.value.s);
					abort();
				}
				printf("%ld", elements[arg_idx].value.i);
				arg_idx--;
			} else if (*p == 'f') {
				// Float argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Not enough arguments for format string\n");
					free(elements);
					free(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_FLOAT) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Expected float for %%f, got type %d\n",
						elements[arg_idx].type);
					free(elements);
					free(fmt_elem.value.s);
					abort();
				}
				printf("%f", elements[arg_idx].value.f);
				arg_idx--;
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

	// Clean up all strings (including format and arguments)
	for (size_t i = 0; i < stack_size; i++) {
		if (elements[i].type == QD_STACK_TYPE_STR) {
			free(elements[i].value.s);
		}
	}
	free(elements);

	return (qd_exec_result){0};
}
