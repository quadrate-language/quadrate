#include <errno.h>
#include <quadrate/fmt/fmt.h>
#include <quadrate/rt/qd_string.h>
#include <quadrate/rt/runtime.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// We intentionally use non-literal format strings since we're implementing printf
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

// Cap for width/precision fields in a format specifier. Width/precision come
// straight from the caller's (untrusted) format string; without a bound a spec
// like "%999999999d" would make the underlying printf attempt a huge amount of
// output/padding, a denial-of-service vector.
#define FMT_MAX_FIELD 1048576

/** One conversion: `%`, flags, width, precision, an ignored length modifier, the letter */
typedef struct {
	const char* start;	// the '%'
	size_t flags_len;	// flags, width and precision, which go to printf as written
	const char* conv;	// the conversion letter
	bool field_too_big; // a width or precision over FMT_MAX_FIELD
} fmt_spec_t;

// The value type a conversion letter takes, or 0 for a letter that is not a conversion:
// 'i' i64, 'f' f64, 's' str, 'p' ptr
static char spec_type(char conv) {
	switch (conv) {
	case 'd':
	case 'i':
	case 'x':
	case 'X':
	case 'o':
	case 'u':
	case 'c':
		return 'i';
	case 'f':
	case 'e':
	case 'E':
	case 'g':
	case 'G':
		return 'f';
	case 's':
		return 's';
	case 'p':
		return 'p';
	default:
		return 0;
	}
}

static long read_field(const char** p, bool* too_big) {
	long n = 0;
	while (**p >= '0' && **p <= '9') {
		if (n <= FMT_MAX_FIELD) {
			n = n * 10 + (**p - '0');
		}
		(*p)++;
	}
	if (n > FMT_MAX_FIELD) {
		*too_big = true;
	}
	return n;
}

// Parse the conversion at `p`, just past a '%' that is not "%%"
static fmt_spec_t parse_spec(const char* p) {
	fmt_spec_t spec = {p - 1, 0, NULL, false};
	while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') {
		p++;
	}
	read_field(&p, &spec.field_too_big);
	if (*p == '.') {
		p++;
		read_field(&p, &spec.field_too_big);
	}
	spec.flags_len = (size_t)(p - spec.start - 1);
	// The length is ours to choose, from the value's type: a written l, ll, h, hh, L, z, j or t goes
	if (*p == 'l' || *p == 'h') {
		p += p[1] == p[0] ? 2 : 1;
	} else if (*p == 'L' || *p == 'z' || *p == 'j' || *p == 't') {
		p++;
	}
	spec.conv = p;
	return spec;
}

// Walk `format` and call `each` for every conversion, in order. Stops early when it returns false.
static void for_each_spec(const char* format, bool (*each)(const fmt_spec_t* spec, void* data), void* data) {
	for (const char* p = format; *p; p++) {
		if (*p != '%') {
			continue;
		}
		if (p[1] == '%') {
			// "%%" takes no value and writes one '%'
			const fmt_spec_t percent = {p, 0, p + 1, false};
			if (!each(&percent, data)) {
				return;
			}
			p++;
			continue;
		}
		const fmt_spec_t spec = parse_spec(p + 1);
		if (spec_type(*spec.conv) && !each(&spec, data)) {
			return;
		}
		if (spec_type(*spec.conv)) {
			p = spec.conv;
		}
	}
}

typedef struct {
	qd_context* ctx;
	const char* op;
	const qd_stack_element_t* args; // the first argument, deepest on the stack
	size_t count;					// how many the format names
	size_t checked;
	const char* error;
	char error_buf[160];
} fmt_check_t;

static bool count_spec(const fmt_spec_t* spec, void* data) {
	fmt_check_t* check = (fmt_check_t*)data;
	if (*spec->conv == '%') {
		return true;
	}
	if (spec->field_too_big) {
		check->error = "format width/precision too large";
	}
	check->count++;
	return check->error == NULL;
}

static bool check_spec(const fmt_spec_t* spec, void* data) {
	fmt_check_t* check = (fmt_check_t*)data;
	if (*spec->conv == '%') {
		return true;
	}
	const qd_stack_type actual = check->args[check->checked].type;
	const char want = spec_type(*spec->conv);
	const bool ok = (want == 'i' && actual == QD_STACK_TYPE_INT) ||
					(want == 'f' && (actual == QD_STACK_TYPE_FLOAT || actual == QD_STACK_TYPE_INT)) ||
					(want == 's' && actual == QD_STACK_TYPE_STR) || (want == 'p' && actual == QD_STACK_TYPE_PTR);
	if (!ok) {
		const char* expected = want == 'i' ? "i64" : want == 'f' ? "f64" : want == 's' ? "str" : "ptr";
		snprintf(check->error_buf, sizeof(check->error_buf), "Expected %s for %%%c (argument %zu), got %s", expected,
				*spec->conv, check->checked + 1, qd_type_name(actual));
		check->error = check->error_buf;
	}
	check->checked++;
	return ok;
}

__attribute__((format(printf, 2, 3))) static bool append_printf(qd_string_builder_t* sb, const char* spec, ...) {
	va_list args;
	va_start(args, spec);
	const int n = vsnprintf(NULL, 0, spec, args);
	va_end(args);
	if (n < 0) {
		return false;
	}
	char* text = malloc((size_t)n + 1);
	if (!text) {
		return false;
	}
	va_start(args, spec);
	vsnprintf(text, (size_t)n + 1, spec, args);
	va_end(args);
	const bool ok = qd_sb_append(sb, text, (size_t)n);
	free(text);
	return ok;
}

typedef struct {
	qd_string_builder_t* sb;
	const qd_stack_element_t* args;
	size_t next;
	const char* copied_to; // the format, written out up to here
	bool ok;
} fmt_write_t;

static bool write_spec(const fmt_spec_t* spec, void* data) {
	fmt_write_t* w = (fmt_write_t*)data;
	w->ok = w->ok && qd_sb_append(w->sb, w->copied_to, (size_t)(spec->start - w->copied_to));
	w->copied_to = spec->conv + 1;
	if (*spec->conv == '%') {
		w->ok = w->ok && qd_sb_append(w->sb, "%", 1);
		return true;
	}

	// '%', flags, width, precision, then our length modifier and the letter
	char printf_spec[48];
	const char conv = *spec->conv;
	const char want = spec_type(conv);
	const char* length = (want == 'i' && conv != 'c') ? "ll" : "";
	snprintf(printf_spec, sizeof(printf_spec), "%%%.*s%s%c", (int)spec->flags_len, spec->start + 1, length, conv);

	const qd_stack_element_t* arg = &w->args[w->next++];
	switch (want) {
	case 'i':
		if (conv == 'c') {
			w->ok = w->ok && append_printf(w->sb, printf_spec, (int)arg->value.i);
		} else if (conv == 'd' || conv == 'i') {
			w->ok = w->ok && append_printf(w->sb, printf_spec, (long long)arg->value.i);
		} else {
			w->ok = w->ok && append_printf(w->sb, printf_spec, (unsigned long long)arg->value.i);
		}
		break;
	case 'f':
		w->ok = w->ok &&
				append_printf(w->sb, printf_spec, arg->type == QD_STACK_TYPE_INT ? (double)arg->value.i : arg->value.f);
		break;
	case 's':
		w->ok = w->ok && append_printf(w->sb, printf_spec, qd_string_data(arg->value.s));
		break;
	default:
		w->ok = w->ok && append_printf(w->sb, printf_spec, arg->value.p);
		break;
	}
	return true;
}

// Format the string on top of the stack with the values under it, one per conversion,
// the first deepest, and pop them all. The format and every argument are checked before
// anything is popped, so a raise leaves the stack as it was.
static qd_string_t* format_from_stack(qd_context* ctx, const char* op) {
	qd_stack* st = ctx->st;
	if (st->size == 0) {
		qd_fatal_raise(ctx, op, "Stack underflow");
	}
	const qd_stack_element_t* fmt_elem = &st->data[st->size - 1];
	if (fmt_elem->type != QD_STACK_TYPE_STR) {
		qd_fatal_raise(ctx, op, "Expected a format string, got %s", qd_type_name(fmt_elem->type));
	}
	const char* format = qd_string_data(fmt_elem->value.s);

	fmt_check_t check = {ctx, op, NULL, 0, 0, NULL, {0}};
	for_each_spec(format, count_spec, &check);
	if (check.error) {
		qd_fatal_raise(ctx, op, "%s", check.error);
	}
	if (st->size - 1 < check.count) {
		qd_fatal_raise(ctx, op, "The format takes %zu values, the stack has %zu", check.count, st->size - 1);
	}
	const size_t base = st->size - 1 - check.count;
	check.args = &st->data[base];
	for_each_spec(format, check_spec, &check);
	if (check.error) {
		qd_fatal_raise(ctx, op, "%s", check.error);
	}

	qd_string_builder_t* sb = qd_sb_create(strlen(format) + 64);
	fmt_write_t w = {sb, check.args, 0, format, sb != NULL};
	if (sb) {
		for_each_spec(format, write_spec, &w);
		w.ok = w.ok && qd_sb_append(sb, w.copied_to, strlen(w.copied_to));
	}
	qd_string_t* result = w.ok ? qd_sb_to_string(sb) : NULL;
	qd_sb_free(sb);
	if (!result) {
		qd_fatal_raise(ctx, op, "Out of memory");
	}

	for (size_t i = base; i < st->size; i++) {
		if (st->data[i].type == QD_STACK_TYPE_STR) {
			qd_string_release(st->data[i].value.s);
		}
	}
	st->size = base;
	return result;
}

static void write_string(FILE* f, const qd_string_t* s) {
	fwrite(qd_string_data(s), 1, qd_string_length(s), f);
}

// Stack signature: ( args... format:str -- )
int usr_fmt_printf(qd_context* ctx) {
	qd_string_t* text = format_from_stack(ctx, "fmt::printf");
	write_string(stdout, text);
	qd_string_release(text);
	return 0;
}

// Stack signature: ( args... format:str -- result:str )
int usr_fmt_sprintf(qd_context* ctx) {
	qd_string_t* text = format_from_stack(ctx, "fmt::sprintf");
	qd_push_s_ref(ctx, text);
	qd_string_release(text);
	return 0;
}

// Stack signature: ( args... fd:i64 format:str -- )
// The format is on top, as the signature has it, with the fd under it and the values
// under that. This took the fd from the top, so no call written as declared worked.
int usr_fmt_fprintf(qd_context* ctx) {
	qd_stack* st = ctx->st;
	if (st->size < 2 || st->data[st->size - 2].type != QD_STACK_TYPE_INT) {
		qd_fatal_raise(ctx, "fmt::fprintf", "Expected an fd (i64) under the format string");
	}
	// Set the fd aside, so the values sit right under the format
	const qd_stack_element_t format = st->data[st->size - 1];
	const int fd = (int)st->data[st->size - 2].value.i;
	st->data[st->size - 2] = format;
	st->size--;
	qd_string_t* text = format_from_stack(ctx, "fmt::fprintf");

	// stdio may still hold output for the same descriptor, which goes first
	fflush(NULL);
	const char* out = qd_string_data(text);
	const size_t len = qd_string_length(text);
	size_t written = 0;
	while (written < len) {
		const ssize_t n = write(fd, out + written, len - written);
		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n <= 0) {
			break;
		}
		written += (size_t)n;
	}
	qd_string_release(text);
	return 0;
}

int usr_fmt_print(qd_context* ctx) {
	qd_string_t* s;
	qd_args(ctx, "fmt::print", "s", &s);
	write_string(stdout, s);
	qd_string_release(s);
	return 0;
}

int usr_fmt_println(qd_context* ctx) {
	qd_string_t* s;
	qd_args(ctx, "fmt::println", "s", &s);
	write_string(stdout, s);
	fputc('\n', stdout);
	qd_string_release(s);
	return 0;
}

int usr_fmt_eprintln(qd_context* ctx) {
	qd_string_t* s;
	qd_args(ctx, "fmt::eprintln", "s", &s);
	// What went to stdout before this came first
	fflush(stdout);
	write_string(stderr, s);
	fputc('\n', stderr);
	qd_string_release(s);
	return 0;
}

int usr_fmt_sprintln(qd_context* ctx) {
	qd_string_t* s;
	qd_args(ctx, "fmt::sprintln", "s", &s);
	const size_t len = qd_string_length(s);
	char* data = malloc(len + 1);
	if (!data) {
		qd_string_release(s);
		qd_fatal_raise(ctx, "fmt::sprintln", "Out of memory");
	}
	memcpy(data, qd_string_data(s), len);
	data[len] = '\n';
	qd_string_release(s);
	qd_string_t* result = qd_string_create_with_length(data, len + 1);
	free(data);
	qd_push_s_ref(ctx, result);
	qd_string_release(result);
	return 0;
}

#pragma GCC diagnostic pop
