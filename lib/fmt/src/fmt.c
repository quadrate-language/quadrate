#include <quadrate/fmt/fmt.h>
#include <quadrate/rt/qd_string.h>
#include <quadrate/rt/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// We intentionally use non-literal format strings since we're implementing printf
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

// Helper function to skip past format specifier modifiers (flags, width, precision)
// Returns pointer to the conversion specifier character
static const char* skip_format_modifiers(const char* p) {
	// Skip flags: -, +, space, #, 0
	while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') {
		p++;
	}
	// Skip width
	while (*p >= '0' && *p <= '9') {
		p++;
	}
	// Skip precision
	if (*p == '.') {
		p++;
		while (*p >= '0' && *p <= '9') {
			p++;
		}
	}
	// Skip length modifiers (l, ll, h, hh, L, z, j, t)
	if (*p == 'l') {
		p++;
		if (*p == 'l') p++;
	} else if (*p == 'h') {
		p++;
		if (*p == 'h') p++;
	} else if (*p == 'L' || *p == 'z' || *p == 'j' || *p == 't') {
		p++;
	}
	return p;
}

// Helper function to count format specifiers in format string
static int count_format_specifiers(const char* fmt) {
	int count = 0;
	for (const char* p = fmt; *p; p++) {
		if (*p == '%') {
			p++; // Skip '%'
			if (*p == '%') {
				// Literal '%', not a format specifier
				continue;
			}
			// Skip modifiers to find the conversion specifier
			p = skip_format_modifiers(p);
			if (*p == 's' || *p == 'd' || *p == 'i' || *p == 'f' ||
			    *p == 'e' || *p == 'E' || *p == 'g' || *p == 'G' ||
			    *p == 'x' || *p == 'X' || *p == 'o' || *p == 'u' ||
			    *p == 'c' || *p == 'p') {
				count++;
			}
		}
	}
	return count;
}

int usr_fmt_printf(qd_context* ctx) {
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
			const char* spec_start = p; // Remember start of format specifier
			p++; // Skip '%'

			if (*p == '%') {
				// Literal '%'
				putchar('%');
				continue;
			}

			// Skip to conversion specifier
			const char* conv = skip_format_modifiers(p);
			char conv_char = *conv;

			// Extract the full format specifier (e.g., "%.2f")
			size_t spec_len = (size_t)(conv - spec_start + 1);
			char spec_buf[64];
			if (spec_len >= sizeof(spec_buf)) spec_len = sizeof(spec_buf) - 1;
			memcpy(spec_buf, spec_start, spec_len);
			spec_buf[spec_len] = '\0';

			if (conv_char == 's') {
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
				printf(spec_buf, qd_string_data(elements[arg_idx].value.s));
				arg_idx--;
			} else if (conv_char == 'd' || conv_char == 'i' || conv_char == 'x' ||
			           conv_char == 'X' || conv_char == 'o' || conv_char == 'u') {
				// Integer argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Not enough arguments for format string\n");
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_INT) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Expected int for %%%c, got type %d\n",
						conv_char, elements[arg_idx].type);
					if (elements) {
						for (int i = 0; i < arg_count; i++) {
							if (elements[i].type == QD_STACK_TYPE_STR) qd_string_release(elements[i].value.s);
						}
						free(elements);
					}
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				// Insert 'l' modifier for long if not already present
				char int_spec[68];
				size_t idx = spec_len - 1;
				memcpy(int_spec, spec_buf, idx);
				int_spec[idx++] = 'l';
				int_spec[idx++] = conv_char;
				int_spec[idx] = '\0';
				printf(int_spec, elements[arg_idx].value.i);
				arg_idx--;
			} else if (conv_char == 'f' || conv_char == 'e' || conv_char == 'E' ||
			           conv_char == 'g' || conv_char == 'G') {
				// Float argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Not enough arguments for format string\n");
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_FLOAT) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Expected float for %%%c, got type %d\n",
						conv_char, elements[arg_idx].type);
					if (elements) {
						for (int i = 0; i < arg_count; i++) {
							if (elements[i].type == QD_STACK_TYPE_STR) qd_string_release(elements[i].value.s);
						}
						free(elements);
					}
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				printf(spec_buf, elements[arg_idx].value.f);
				arg_idx--;
			} else if (conv_char == 'c') {
				// Character argument (from int)
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Not enough arguments for format string\n");
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_INT) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Expected int for %%c, got type %d\n",
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
				printf(spec_buf, (int)elements[arg_idx].value.i);
				arg_idx--;
			} else if (conv_char == 'p') {
				// Pointer argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Not enough arguments for format string\n");
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_PTR) {
					fprintf(stderr, "Fatal error in usr_fmt_printf: Expected ptr for %%p, got type %d\n",
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
				printf(spec_buf, elements[arg_idx].value.p);
				arg_idx--;
			} else {
				// Unknown format specifier, just print it
				putchar('%');
				putchar(*p);
				continue;
			}
			p = conv; // Advance past the conversion character
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

	return (int){0};
}

int usr_fmt_sprintf(qd_context* ctx) {
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
			const char* spec_start = p; // Remember start of format specifier
			p++; // Skip '%'

			if (*p == '%') {
				// Literal '%'
				qd_sb_append(sb, "%", 1);
				continue;
			}

			// Skip to conversion specifier
			const char* conv = skip_format_modifiers(p);
			char conv_char = *conv;

			// Extract the full format specifier (e.g., "%.2f")
			size_t spec_len = (size_t)(conv - spec_start + 1);
			char spec_buf[64];
			if (spec_len >= sizeof(spec_buf)) spec_len = sizeof(spec_buf) - 1;
			memcpy(spec_buf, spec_start, spec_len);
			spec_buf[spec_len] = '\0';

			char result_buf[256];
			int len = 0;

			if (conv_char == 's') {
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
				len = snprintf(result_buf, sizeof(result_buf), spec_buf, qd_string_data(elements[arg_idx].value.s));
				qd_sb_append(sb, result_buf, (size_t)len);
				arg_idx--;
			} else if (conv_char == 'd' || conv_char == 'i' || conv_char == 'x' ||
			           conv_char == 'X' || conv_char == 'o' || conv_char == 'u') {
				// Integer argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Not enough arguments for format string\n");
					qd_sb_free(sb);
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_INT) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Expected int for %%%c, got type %d\n",
						conv_char, elements[arg_idx].type);
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
				// Insert 'l' modifier for long
				char int_spec[68];
				size_t idx = spec_len - 1;
				memcpy(int_spec, spec_buf, idx);
				int_spec[idx++] = 'l';
				int_spec[idx++] = conv_char;
				int_spec[idx] = '\0';
				len = snprintf(result_buf, sizeof(result_buf), int_spec, elements[arg_idx].value.i);
				qd_sb_append(sb, result_buf, (size_t)len);
				arg_idx--;
			} else if (conv_char == 'f' || conv_char == 'e' || conv_char == 'E' ||
			           conv_char == 'g' || conv_char == 'G') {
				// Float argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Not enough arguments for format string\n");
					qd_sb_free(sb);
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_FLOAT) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Expected float for %%%c, got type %d\n",
						conv_char, elements[arg_idx].type);
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
				len = snprintf(result_buf, sizeof(result_buf), spec_buf, elements[arg_idx].value.f);
				qd_sb_append(sb, result_buf, (size_t)len);
				arg_idx--;
			} else if (conv_char == 'c') {
				// Character argument (from int)
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Not enough arguments for format string\n");
					qd_sb_free(sb);
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_INT) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Expected int for %%c, got type %d\n",
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
				len = snprintf(result_buf, sizeof(result_buf), spec_buf, (int)elements[arg_idx].value.i);
				qd_sb_append(sb, result_buf, (size_t)len);
				arg_idx--;
			} else if (conv_char == 'p') {
				// Pointer argument
				if (arg_idx < 0) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Not enough arguments for format string\n");
					qd_sb_free(sb);
					free(elements);
					qd_string_release(fmt_elem.value.s);
					abort();
				}
				if (elements[arg_idx].type != QD_STACK_TYPE_PTR) {
					fprintf(stderr, "Fatal error in usr_fmt_sprintf: Expected ptr for %%p, got type %d\n",
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
				len = snprintf(result_buf, sizeof(result_buf), spec_buf, elements[arg_idx].value.p);
				qd_sb_append(sb, result_buf, (size_t)len);
				arg_idx--;
			} else {
				// Unknown format specifier, just append it
				qd_sb_append(sb, "%", 1);
				qd_sb_append(sb, p, 1);
				continue;
			}
			p = conv; // Advance past the conversion character
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

	return (int){0};
}

int usr_fmt_print(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in usr_fmt_print: Expected string on stack\n");
		abort();
	}
	fputs(qd_string_data(elem.value.s), stdout);
	qd_string_release(elem.value.s);
	return (int){0};
}

int usr_fmt_println(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in usr_fmt_println: Expected string on stack\n");
		abort();
	}
	puts(qd_string_data(elem.value.s));
	qd_string_release(elem.value.s);
	return (int){0};
}

int usr_fmt_eprintln(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in usr_fmt_eprintln: Expected string on stack\n");
		abort();
	}
	fprintf(stderr, "%s\n", qd_string_data(elem.value.s));
	qd_string_release(elem.value.s);
	return (int){0};
}

int usr_fmt_sprintln(qd_context* ctx) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK || elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in usr_fmt_sprintln: Expected string on stack\n");
		abort();
	}
	const char* data = qd_string_data(elem.value.s);
	size_t len = strlen(data);
	qd_string_builder_t* sb = qd_sb_create(len + 2);
	qd_sb_append(sb, data, len);
	qd_sb_append(sb, "\n", 1);
	qd_string_t* result = qd_sb_to_string(sb);
	qd_sb_free(sb);
	qd_string_release(elem.value.s);
	err = qd_stack_push_str(ctx->st, qd_string_data(result));
	qd_string_release(result);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in usr_fmt_sprintln: Failed to push result\n");
		abort();
	}
	return (int){0};
}

int usr_fmt_fprintf(qd_context* ctx) {
	// Stack: ( arg1 arg2 ... argN format:s fd:i64 -- )
	// Pop fd first, then delegate to printf-like logic writing to fd

	qd_stack_element_t fd_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &fd_elem);
	if (err != QD_STACK_OK || fd_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in usr_fmt_fprintf: Expected fd (i64) on stack\n");
		abort();
	}
	int fd = (int)fd_elem.value.i;

	// Pop format string
	qd_stack_element_t fmt_elem;
	err = qd_stack_pop(ctx->st, &fmt_elem);
	if (err != QD_STACK_OK || fmt_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in usr_fmt_fprintf: Expected format string on stack\n");
		abort();
	}

	const char* format = qd_string_data(fmt_elem.value.s);
	int arg_count = count_format_specifiers(format);

	qd_stack_element_t* elements = NULL;
	if (arg_count > 0) {
		elements = malloc(sizeof(qd_stack_element_t) * (size_t)arg_count);
		if (!elements) {
			qd_string_release(fmt_elem.value.s);
			abort();
		}
		for (int i = 0; i < arg_count; i++) {
			err = qd_stack_pop(ctx->st, &elements[i]);
			if (err != QD_STACK_OK) {
				for (int j = 0; j < i; j++) {
					if (elements[j].type == QD_STACK_TYPE_STR) qd_string_release(elements[j].value.s);
				}
				free(elements);
				qd_string_release(fmt_elem.value.s);
				abort();
			}
		}
	}

	// Build formatted string using sprintf logic, then write to fd
	qd_string_builder_t* sb = qd_sb_create(256);
	int arg_idx = arg_count - 1;
	for (const char* p = format; *p; p++) {
		if (*p == '%' && *(p + 1)) {
			const char* spec_start = p;
			p++;
			if (*p == '%') { qd_sb_append(sb, "%", 1); continue; }
			const char* conv = skip_format_modifiers(p);
			char conv_char = *conv;
			size_t spec_len = (size_t)(conv - spec_start + 1);
			char spec_buf[64];
			if (spec_len >= sizeof(spec_buf)) spec_len = sizeof(spec_buf) - 1;
			memcpy(spec_buf, spec_start, spec_len);
			spec_buf[spec_len] = '\0';
			char result_buf[256];
			int len = 0;
			if (arg_idx >= 0) {
				if (conv_char == 's' && elements[arg_idx].type == QD_STACK_TYPE_STR) {
					len = snprintf(result_buf, sizeof(result_buf), spec_buf, qd_string_data(elements[arg_idx].value.s));
					arg_idx--;
				} else if ((conv_char == 'd' || conv_char == 'i' || conv_char == 'x' || conv_char == 'X' || conv_char == 'o' || conv_char == 'u') && elements[arg_idx].type == QD_STACK_TYPE_INT) {
					char int_spec[68];
					size_t idx = spec_len - 1;
					memcpy(int_spec, spec_buf, idx);
					int_spec[idx++] = 'l';
					int_spec[idx++] = conv_char;
					int_spec[idx] = '\0';
					len = snprintf(result_buf, sizeof(result_buf), int_spec, elements[arg_idx].value.i);
					arg_idx--;
				} else if ((conv_char == 'f' || conv_char == 'e' || conv_char == 'E' || conv_char == 'g' || conv_char == 'G') && elements[arg_idx].type == QD_STACK_TYPE_FLOAT) {
					len = snprintf(result_buf, sizeof(result_buf), spec_buf, elements[arg_idx].value.f);
					arg_idx--;
				} else if (conv_char == 'c' && elements[arg_idx].type == QD_STACK_TYPE_INT) {
					len = snprintf(result_buf, sizeof(result_buf), spec_buf, (int)elements[arg_idx].value.i);
					arg_idx--;
				} else if (conv_char == 'p' && elements[arg_idx].type == QD_STACK_TYPE_PTR) {
					len = snprintf(result_buf, sizeof(result_buf), spec_buf, elements[arg_idx].value.p);
					arg_idx--;
				}
			}
			if (len > 0) qd_sb_append(sb, result_buf, (size_t)len);
			p = conv;
		} else {
			qd_sb_append(sb, p, 1);
		}
	}

	qd_string_t* result = qd_sb_to_string(sb);
	qd_sb_free(sb);
	if (result) {
		const char* out = qd_string_data(result);
		size_t out_len = strlen(out);
		// Use write() for fd-based output
		write(fd, out, out_len);
		qd_string_release(result);
	}

	if (elements) {
		for (int i = 0; i < arg_count; i++) {
			if (elements[i].type == QD_STACK_TYPE_STR) qd_string_release(elements[i].value.s);
		}
		free(elements);
	}
	qd_string_release(fmt_elem.value.s);
	return (int){0};
}

#pragma GCC diagnostic pop
