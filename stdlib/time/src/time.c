#define _DEFAULT_SOURCE
#include "platform/time_platform.h"
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <quadrate/time/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TIME_ERR_OK 1
#define TIME_ERR_FORMAT 2
#define TIME_ERR_PARSE 3

// unix - get current Unix timestamp in seconds ( -- timestamp:i64 )
int usr_time_unix(qd_context* ctx) {
	int64_t timestamp = time_platform_unix();

	qd_stack_error err = qd_stack_push_int(ctx->st, timestamp);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in time::unix: Stack overflow\n");
		abort();
	}

	return (int){0};
}

// now - get current time in nanoseconds since epoch ( -- nanoseconds:i64 )
int usr_time_now(qd_context* ctx) {
	int64_t nanoseconds = time_platform_now_ns();

	qd_stack_error err = qd_stack_push_int(ctx->st, nanoseconds);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in time::now: Stack overflow\n");
		abort();
	}

	return (int){0};
}

// sleep - sleep for N nanoseconds ( nanoseconds:i -- )
int usr_time_sleep(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in time::sleep: Stack underflow\n");
		abort();
	}

	if (val.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in time::sleep: Expected integer, got type %d\n", val.type);
		abort();
	}

	if (val.value.i < 0) {
		fprintf(stderr, "Fatal error in time::sleep: Duration cannot be negative\n");
		abort();
	}

	time_platform_sleep_ns(val.value.i);

	return (int){0};
}

// format - format timestamp using strftime ( timestamp:i format:s -- result:s )!
int usr_time_format(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in time::format: Stack underflow (need 2, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop format string
	qd_stack_element_t format_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &format_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in time::format: Failed to pop format\n");
		abort();
	}
	if (format_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in time::format: Expected string for format, got %d\n", format_elem.type);
		abort();
	}

	// Pop timestamp
	qd_stack_element_t ts_elem;
	err = qd_stack_pop(ctx->st, &ts_elem);
	if (err != QD_STACK_OK) {
		qd_string_release(format_elem.value.s);
		fprintf(stderr, "Fatal error in time::format: Failed to pop timestamp\n");
		abort();
	}
	if (ts_elem.type != QD_STACK_TYPE_INT) {
		qd_string_release(format_elem.value.s);
		fprintf(stderr, "Fatal error in time::format: Expected integer for timestamp, got %d\n", ts_elem.type);
		abort();
	}

	const char* format = qd_string_data(format_elem.value.s);
	if (!format) {
		qd_string_release(format_elem.value.s);
		ctx->error_code = TIME_ERR_FORMAT;
		qd_set_error_msg(ctx, "time::format: null format string");
		qd_stack_push_int(ctx->st, (int64_t)TIME_ERR_FORMAT);
		return (int){TIME_ERR_FORMAT};
	}

	time_t timestamp = (time_t)ts_elem.value.i;
	struct tm tm_info;
	if (gmtime_r(&timestamp, &tm_info) == NULL) {
		qd_string_release(format_elem.value.s);
		ctx->error_code = TIME_ERR_FORMAT;
		qd_set_error_msg(ctx, "time::format: invalid timestamp");
		qd_stack_push_int(ctx->st, (int64_t)TIME_ERR_FORMAT);
		return (int){TIME_ERR_FORMAT};
	}

	// Buffer for strftime result (should be enough for most formats)
	char buffer[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	size_t len = strftime(buffer, sizeof(buffer), format, &tm_info);
#pragma GCC diagnostic pop
	if (len == 0 && format[0] != '\0') {
		// strftime returns 0 if buffer too small or format error
		qd_string_release(format_elem.value.s);
		ctx->error_code = TIME_ERR_FORMAT;
		qd_set_error_msg(ctx, "time::format: format error or buffer overflow");
		qd_stack_push_int(ctx->st, (int64_t)TIME_ERR_FORMAT);
		return (int){TIME_ERR_FORMAT};
	}

	qd_string_release(format_elem.value.s);
	qd_stack_push_str(ctx->st, buffer);
	qd_stack_push_int(ctx->st, TIME_ERR_OK);
	return (int){0};
}

// parse - parse string to timestamp using strptime ( str:s format:s -- timestamp:i )!
int usr_time_parse(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in time::parse: Stack underflow (need 2, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop format string
	qd_stack_element_t format_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &format_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in time::parse: Failed to pop format\n");
		abort();
	}
	if (format_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in time::parse: Expected string for format, got %d\n", format_elem.type);
		abort();
	}

	// Pop input string
	qd_stack_element_t str_elem;
	err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		qd_string_release(format_elem.value.s);
		fprintf(stderr, "Fatal error in time::parse: Failed to pop input string\n");
		abort();
	}
	if (str_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(format_elem.value.s);
		fprintf(stderr, "Fatal error in time::parse: Expected string for input, got %d\n", str_elem.type);
		abort();
	}

	const char* format = qd_string_data(format_elem.value.s);
	const char* input = qd_string_data(str_elem.value.s);
	if (!format || !input) {
		qd_string_release(format_elem.value.s);
		qd_string_release(str_elem.value.s);
		ctx->error_code = TIME_ERR_PARSE;
		qd_set_error_msg(ctx, "time::parse: null string");
		qd_stack_push_int(ctx->st, (int64_t)TIME_ERR_PARSE);
		return (int){TIME_ERR_PARSE};
	}

	struct tm tm_info;
	memset(&tm_info, 0, sizeof(tm_info));
	// Default to DST unknown
	tm_info.tm_isdst = -1;

	char* result = strptime(input, format, &tm_info);
	if (result == NULL) {
		qd_string_release(format_elem.value.s);
		qd_string_release(str_elem.value.s);
		ctx->error_code = TIME_ERR_PARSE;
		qd_set_error_msg(ctx, "time::parse: parse failed");
		qd_stack_push_int(ctx->st, (int64_t)TIME_ERR_PARSE);
		return (int){TIME_ERR_PARSE};
	}

	time_t timestamp = timegm(&tm_info);
	if (timestamp == (time_t)-1) {
		qd_string_release(format_elem.value.s);
		qd_string_release(str_elem.value.s);
		ctx->error_code = TIME_ERR_PARSE;
		qd_set_error_msg(ctx, "time::parse: invalid time");
		qd_stack_push_int(ctx->st, (int64_t)TIME_ERR_PARSE);
		return (int){TIME_ERR_PARSE};
	}

	qd_string_release(format_elem.value.s);
	qd_string_release(str_elem.value.s);
	qd_stack_push_int(ctx->st, (int64_t)timestamp);
	qd_stack_push_int(ctx->st, TIME_ERR_OK);
	return (int){0};
}
