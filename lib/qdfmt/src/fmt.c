#include <qdfmt/fmt.h>
#include <qdrt/qd_string.h>
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

	const char* format = qd_string_data(fmt_elem.value.s);
	int arg_count = count_format_specifiers(format);

	// Now pop exactly arg_count arguments from stack
	qd_stack_element_t* elements = NULL;
	if (arg_count > 0) {
		elements = malloc(sizeof(qd_stack_element_t) * (size_t)arg_count);
		if (!elements) {
			fprintf(stderr, "Fatal error in usr_fmt_printf: Memory allocation failed\n");
			qd_string_release(fmt_elem.value.s);
			abort();
		}

		for (int i = 0; i < arg_count; i++) {
			err = qd_stack_pop(ctx->st, &elements[i]);
			if (err != QD_STACK_OK) {
				fprintf(stderr, "Fatal error in usr_fmt_printf: Not enough arguments on stack\n");
				for (int j = 0; j < i; j++) {
					if (elements[j].type == QD_STACK_TYPE_STR) qd_string_release(elements[j].value.s);
				}
				free(elements);
				qd_string_release(fmt_elem.value.s);
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
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_STR) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Expected string for %%s, got type %d\n",
						elements[arg_idx].type);
					if (elements) {
						for (int i = 0; i < arg_count; i++) {
							if (elements[i].type == QD_STACK_TYPE_STR) qd_string_release(elements[i].value.s);
						}
						free(elements);
					}
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				printf("%s", qd_string_data(elements[arg_idx].value.s));
				arg_idx--;
			} else if (*p == 'd' || *p == 'i') {
				// Integer argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Not enough arguments for format string\n");
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_INT) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Expected int for %%d, got type %d\n",
						elements[arg_idx].type);
					if (elements) {
						for (int i = 0; i < arg_count; i++) {
							if (elements[i].type == QD_STACK_TYPE_STR) qd_string_release(elements[i].value.s);
						}
						free(elements);
					}
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				printf("%ld", elements[arg_idx].value.i);
				arg_idx--;
			} else if (*p == 'f') {
				// Float argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Not enough arguments for format string\n");
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_FLOAT) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Expected float for %%f, got type %d\n",
						elements[arg_idx].type);
					if (elements) {
						for (int i = 0; i < arg_count; i++) {
							if (elements[i].type == QD_STACK_TYPE_STR) qd_string_release(elements[i].value.s);
						}
						free(elements);
					}
					qd_string_release(fmt_elem.value.s);
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
				qd_string_release(elements[i].value.s);
			}
		}
		free(elements);
	}
	qd_string_release(fmt_elem.value.s);

	return (qd_exec_result){0};
}

qd_exec_result usr_fmt_sprintf(qd_context* ctx) {
	// Stack order: ( arg1 arg2 ... argN format:s -- result:s )
	// Format string is on top, arguments are below it

	// Pop format string from top
	qd_stack_element_t fmt_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &fmt_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_fmt_sprintf: Stack underflow\n");
		abort();
	}

	if (fmt_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in usr_fmt_sprintf: Expected format string, got type %d\n", fmt_elem.type);
		abort();
	}

	const char* format = qd_string_data(fmt_elem.value.s);
	int arg_count = count_format_specifiers(format);

	// Pop exactly arg_count arguments from stack
	qd_stack_element_t* elements = NULL;
	if (arg_count > 0) {
		elements = malloc(sizeof(qd_stack_element_t) * (size_t)arg_count);
		if (!elements) {
			fprintf(stderr, "Fatal error in usr_fmt_sprintf: Memory allocation failed\n");
			qd_string_release(fmt_elem.value.s);
			abort();
		}

		for (int i = 0; i < arg_count; i++) {
			err = qd_stack_pop(ctx->st, &elements[i]);
			if (err != QD_STACK_OK) {
				fprintf(stderr, "Fatal error in usr_fmt_sprintf: Not enough arguments on stack\n");
				for (int j = 0; j < i; j++) {
					if (elements[j].type == QD_STACK_TYPE_STR) qd_string_release(elements[j].value.s);
				}
				free(elements);
				qd_string_release(fmt_elem.value.s);
				abort();
			}
		}
	}

	// Create string builder for result
	qd_string_builder_t* sb = qd_sb_create(256);
	if (!sb) {
		fprintf(stderr, "Fatal error in usr_fmt_sprintf: String builder allocation failed\n");
		if (elements) {
			for (int i = 0; i < arg_count; i++) {
				if (elements[i].type == QD_STACK_TYPE_STR) qd_string_release(elements[i].value.s);
			}
			free(elements);
		}
		qd_string_release(fmt_elem.value.s);
		abort();
	}

	// Arguments are in reverse order: elements[arg_count-1] is first arg, elements[0] is last arg
	int arg_idx = arg_count - 1;
	for (const char* p = format; *p; p++) {
		if (*p == '%' && *(p + 1)) {
			p++; // Skip '%'

			if (*p == '%') {
				// Literal '%'
				qd_sb_append(sb, "%", 1);
			} else if (*p == 's') {
				// String argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Not enough arguments for format string\n");
					qd_sb_free(sb);
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_STR) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Expected string for %%s, got type %d\n",
						elements[arg_idx].type);
					qd_sb_free(sb);
					if (elements) {
						for (int i = 0; i < arg_count; i++) {
							if (elements[i].type == QD_STACK_TYPE_STR) qd_string_release(elements[i].value.s);
						}
						free(elements);
					}
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				const char* str_val = qd_string_data(elements[arg_idx].value.s);
				qd_sb_append(sb, str_val, strlen(str_val));
				arg_idx--;
			} else if (*p == 'd' || *p == 'i') {
				// Integer argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Not enough arguments for format string\n");
					qd_sb_free(sb);
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_INT) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Expected int for %%d, got type %d\n",
						elements[arg_idx].type);
					qd_sb_free(sb);
					if (elements) {
						for (int i = 0; i < arg_count; i++) {
							if (elements[i].type == QD_STACK_TYPE_STR) qd_string_release(elements[i].value.s);
						}
						free(elements);
					}
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				char int_buf[32];
				int len = snprintf(int_buf, sizeof(int_buf), "%ld", elements[arg_idx].value.i);
				qd_sb_append(sb, int_buf, (size_t)len);
				arg_idx--;
			} else if (*p == 'f') {
				// Float argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Not enough arguments for format string\n");
					qd_sb_free(sb);
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_FLOAT) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Expected float for %%f, got type %d\n",
						elements[arg_idx].type);
					qd_sb_free(sb);
					if (elements) {
						for (int i = 0; i < arg_count; i++) {
							if (elements[i].type == QD_STACK_TYPE_STR) qd_string_release(elements[i].value.s);
						}
						free(elements);
					}
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				char float_buf[64];
				int len = snprintf(float_buf, sizeof(float_buf), "%f", elements[arg_idx].value.f);
				qd_sb_append(sb, float_buf, (size_t)len);
				arg_idx--;
			} else {
				// Unknown format specifier, just append it
				qd_sb_append(sb, "%", 1);
				qd_sb_append(sb, p, 1);
			}
		} else {
			// Regular character
			qd_sb_append(sb, p, 1);
		}
	}

	// Convert builder to string and push onto stack
	qd_string_t* result = qd_sb_to_string(sb);
	qd_sb_free(sb);

	if (!result) {
		fprintf(stderr, "Fatal error in usr_fmt_sprintf: Failed to create result string\n");
		if (elements) {
			for (int i = 0; i < arg_count; i++) {
				if (elements[i].type == QD_STACK_TYPE_STR) qd_string_release(elements[i].value.s);
			}
			free(elements);
		}
		qd_string_release(fmt_elem.value.s);
		abort();
	}

	// Push result string onto stack
	err = qd_stack_push_str(ctx->st, qd_string_data(result));
	qd_string_release(result);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_fmt_sprintf: Failed to push result onto stack\n");
		if (elements) {
			for (int i = 0; i < arg_count; i++) {
				if (elements[i].type == QD_STACK_TYPE_STR) qd_string_release(elements[i].value.s);
			}
			free(elements);
		}
		qd_string_release(fmt_elem.value.s);
		abort();
	}

	// Clean up all strings (format and arguments)
	if (elements) {
		for (int i = 0; i < arg_count; i++) {
			if (elements[i].type == QD_STACK_TYPE_STR) {
				qd_string_release(elements[i].value.s);
			}
		}
		free(elements);
	}
	qd_string_release(fmt_elem.value.s);

	return (qd_exec_result){0};
}
