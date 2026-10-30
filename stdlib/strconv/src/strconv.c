#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/strconv/strconv.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define STRCONV_ERR_OK 1
#define STRCONV_ERR_INVALID 2

// The parse words take the whole string as the number, as parse_d128 always has: no space
// around it and nothing after it. "12abc" read as 12 and "1.5x" as 1.5, the kind of answer
// that goes unnoticed.

static char digit_to_char(int digit) {
	return (char)(digit < 10 ? '0' + digit : 'a' + (digit - 10));
}

static int char_to_digit(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'z') {
		return 10 + (c - 'a');
	}
	if (c >= 'A' && c <= 'Z') {
		return 10 + (c - 'A');
	}
	return -1;
}

static void check_base(qd_context* ctx, const char* op, int64_t base, qd_string_t* s) {
	if (base < 2 || base > 36) {
		if (s) {
			qd_string_release(s);
		}
		qd_fatal_raise(ctx, op, "Base must be 2-36, got %" PRId64, base);
	}
}

static int fail(qd_context* ctx, const char* op, const char* why, qd_string_t* s) {
	const int code = qd_fail(ctx, STRCONV_ERR_INVALID, "%s: %s: \"%s\"", op, why, qd_string_data(s));
	qd_string_release(s);
	return code;
}

static int push_string(qd_context* ctx, const char* text) {
	qd_push_s(ctx, text);
	return 0;
}

// Stack signature: ( value:i64 base:i64 -- s:str )
int usr_strconv_format_int(qd_context* ctx) {
	int64_t value;
	int64_t base;
	qd_args(ctx, "strconv::format_int", "ii", &value, &base);
	check_base(ctx, "strconv::format_int", base, NULL);

	// 64 binary digits, a sign and the NUL
	char buffer[66];
	char* ptr = buffer + sizeof(buffer) - 1;
	*ptr = '\0';
	// The magnitude as unsigned, so INT64_MIN has one
	uint64_t u = value < 0 ? (uint64_t)0 - (uint64_t)value : (uint64_t)value;
	do {
		*--ptr = digit_to_char((int)(u % (uint64_t)base));
		u /= (uint64_t)base;
	} while (u > 0);
	if (value < 0) {
		*--ptr = '-';
	}
	return push_string(ctx, ptr);
}

// Read an i64 in `base` from all of `s`. 0 on success, else what was wrong.
static const char* read_int(const qd_string_t* s, int64_t base, int64_t* out) {
	const char* p = qd_string_data(s);
	const char* end = p + qd_string_length(s);
	const bool negative = p < end && *p == '-';
	if (p < end && (*p == '-' || *p == '+')) {
		p++;
	}
	if (p == end) {
		return "not a number";
	}
	// INT64_MIN's magnitude is one more than INT64_MAX's
	const uint64_t limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
	uint64_t result = 0;
	for (; p < end; p++) {
		const int digit = char_to_digit(*p);
		if (digit < 0 || digit >= base) {
			return "not a number";
		}
		if (result > (limit - (uint64_t)digit) / (uint64_t)base) {
			return "out of range";
		}
		result = result * (uint64_t)base + (uint64_t)digit;
	}
	*out = negative ? (int64_t)(0 - result) : (int64_t)result;
	return NULL;
}

static int parse_int(qd_context* ctx, const char* op, qd_string_t* s, int64_t base) {
	int64_t value = 0;
	const char* why = read_int(s, base, &value);
	if (why) {
		return fail(ctx, op, why, s);
	}
	qd_string_release(s);
	qd_push_i(ctx, value);
	return qd_push_i(ctx, STRCONV_ERR_OK);
}

// Stack signature: ( s:str base:i64 -- value:i64 )!
int usr_strconv_parse_int(qd_context* ctx) {
	qd_string_t* s;
	int64_t base;
	qd_args(ctx, "strconv::parse_int", "si", &s, &base);
	check_base(ctx, "strconv::parse_int", base, s);
	return parse_int(ctx, "strconv::parse_int", s, base);
}

// Stack signature: ( value:i64 -- s:str )
int usr_strconv_itoa(qd_context* ctx) {
	int64_t value;
	qd_args(ctx, "strconv::itoa", "i", &value);
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%" PRId64, value);
	return push_string(ctx, buffer);
}

// Stack signature: ( s:str -- value:i64 )!
int usr_strconv_atoi(qd_context* ctx) {
	qd_string_t* s;
	qd_args(ctx, "strconv::atoi", "s", &s);
	return parse_int(ctx, "strconv::atoi", s, 10);
}

// Stack signature: ( value:f64 -- s:str )
// The shortest text that reads back as the same double. Plain %g kept six digits, so
// 1234567.0 came out as "1.23457e+06", a different number.
int usr_strconv_format_float(qd_context* ctx) {
	double value;
	qd_args(ctx, "strconv::format_float", "f", &value);
	char buffer[32];
	for (int precision = 15; precision <= 17; precision++) {
		snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
		if (value != value || strtod(buffer, NULL) == value) {
			break;
		}
	}
	return push_string(ctx, buffer);
}

// Stack signature: ( s:str -- value:f64 )!
int usr_strconv_parse_float(qd_context* ctx) {
	qd_string_t* s;
	qd_args(ctx, "strconv::parse_float", "s", &s);
	const char* str = qd_string_data(s);
	const char* end = str + qd_string_length(s);
	if (str == end || isspace((unsigned char)*str)) {
		return fail(ctx, "strconv::parse_float", "not a number", s);
	}
	char* stop;
	errno = 0;
	const double value = strtod(str, &stop);
	if (stop != end) {
		return fail(ctx, "strconv::parse_float", "not a number", s);
	}
	// Too large for a double; one too small to hold rounds to 0 or a subnormal, which is fine
	if (errno == ERANGE && (value > 1.0 || value < -1.0)) {
		return fail(ctx, "strconv::parse_float", "out of range", s);
	}
	qd_string_release(s);
	qd_push_f(ctx, value);
	return qd_push_i(ctx, STRCONV_ERR_OK);
}

// Stack signature: ( s:str -- value:d128 )!
int usr_strconv_parse_d128(qd_context* ctx) {
	qd_string_t* s;
	qd_args(ctx, "strconv::parse_d128", "s", &s);
	qd_d128_t value;
	if (!qd_d128_parse(qd_string_data(s), &value)) {
		return fail(ctx, "strconv::parse_d128", "not a number", s);
	}
	qd_string_release(s);
	qd_push_d(ctx, value);
	return qd_push_i(ctx, STRCONV_ERR_OK);
}

// Stack signature: ( value:i64 -- s:str )
int usr_strconv_format_bool(qd_context* ctx) {
	int64_t value;
	qd_args(ctx, "strconv::format_bool", "i", &value);
	return push_string(ctx, value != 0 ? "true" : "false");
}

// Stack signature: ( s:str -- value:i64 )!
int usr_strconv_parse_bool(qd_context* ctx) {
	qd_string_t* s;
	qd_args(ctx, "strconv::parse_bool", "s", &s);
	const char* str = qd_string_data(s);
	int64_t value;
	if (strcasecmp(str, "true") == 0 || strcmp(str, "1") == 0) {
		value = 1;
	} else if (strcasecmp(str, "false") == 0 || strcmp(str, "0") == 0) {
		value = 0;
	} else {
		return fail(ctx, "strconv::parse_bool", "not a boolean", s);
	}
	qd_string_release(s);
	qd_push_i(ctx, value);
	return qd_push_i(ctx, STRCONV_ERR_OK);
}
