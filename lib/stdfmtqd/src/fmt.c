#include <stdfmtqd/fmt.h>
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
	// Stack order: ( arg1 arg2 ... argN format:s -- )
	// Format string is on top, arguments are below it

	// Pop format string from top
	qd_stack_element_t fmt_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &fmt_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_fmt_printf: Stack underflow\n");
		abort();
	}

	if (fmt_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in usr_fmt_printf: Expected format string, got type %d\n", fmt_elem.type);
		abort();
	}

	const char* format = fmt_elem.value.s;
	int arg_count = count_format_specifiers(format);

	// Now pop exactly arg_count arguments from stack
	qd_stack_element_t* elements = NULL;
	if (arg_count > 0) {
		elements = malloc(sizeof(qd_stack_element_t) * (size_t)arg_count);
		if (!elements) {
			fprintf(stderr, "Fatal error in usr_fmt_printf: Memory allocation failed\n");
			free(fmt_elem.value.s);
			abort();
		}

		for (int i = 0; i < arg_count; i++) {
			err = qd_stack_pop(ctx->st, &elements[i]);
			if (err != QD_STACK_OK) {
				fprintf(stderr, "Fatal error in usr_fmt_printf: Not enough arguments on stack\n");
				for (int j = 0; j < i; j++) {
					if (elements[j].type == QD_STACK_TYPE_STR) free(elements[j].value.s);
				}
				free(elements);
				free(fmt_elem.value.s);
				abort();
			}
		}
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
					if (elements) {
						for (int i = 0; i < arg_count; i++) {
							if (elements[i].type == QD_STACK_TYPE_STR) free(elements[i].value.s);
						}
						free(elements);
					}
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
					if (elements) {
						for (int i = 0; i < arg_count; i++) {
							if (elements[i].type == QD_STACK_TYPE_STR) free(elements[i].value.s);
						}
						free(elements);
					}
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
					if (elements) {
						for (int i = 0; i < arg_count; i++) {
							if (elements[i].type == QD_STACK_TYPE_STR) free(elements[i].value.s);
						}
						free(elements);
					}
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
	if (elements) {
		for (int i = 0; i < arg_count; i++) {
			if (elements[i].type == QD_STACK_TYPE_STR) {
				free(elements[i].value.s);
			}
		}
		free(elements);
	}
	free(fmt_elem.value.s);

	return (qd_exec_result){0};
}
