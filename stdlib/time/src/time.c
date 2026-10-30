// strptime() is an XSI extension. Declared here rather than inherited: the
// module used to get -D_GNU_SOURCE by accident, via llvm-config's flags
// leaking through the compiler dependency it no longer has.
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include "platform/time_platform.h"
#include <inttypes.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/time/time.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TIME_ERR_OK 1
#define TIME_ERR_FORMAT 2
#define TIME_ERR_PARSE 3

// Stack signature: ( -- timestamp:i64 )
int usr_time_unix(qd_context* ctx) {
	return qd_push_i(ctx, time_platform_unix());
}

// Stack signature: ( -- nanoseconds:i64 )
int usr_time_now(qd_context* ctx) {
	return qd_push_i(ctx, time_platform_now_ns());
}

// Stack signature: ( nanoseconds:i64 -- )
int usr_time_sleep(qd_context* ctx) {
	int64_t duration;
	qd_args(ctx, "time::sleep", "i", &duration);
	if (duration < 0) {
		qd_fatal_raise(ctx, "time::sleep", "Duration cannot be negative");
	}
	time_platform_sleep_ns(duration);
	return 0;
}

// Stack signature: ( timestamp:i64 format:str -- result:str )!
int usr_time_format(qd_context* ctx) {
	int64_t ts;
	qd_string_t* format;
	qd_args(ctx, "time::format", "is", &ts, &format);
	const char* fmt = qd_string_data(format);

	const time_t timestamp = (time_t)ts;
	struct tm tm_info;
	if (gmtime_r(&timestamp, &tm_info) == NULL) {
		qd_string_release(format);
		return qd_fail(ctx, TIME_ERR_FORMAT, "time::format: %" PRId64 " is out of range", ts);
	}

	// strftime says 0 both for a result that does not fit and for an empty one, so grow the
	// buffer until it fits; a 256-byte one failed for any longer result
	char* buffer = NULL;
	size_t len = 0;
	for (size_t cap = 256; cap <= 65536; cap *= 4) {
		char* grown = realloc(buffer, cap);
		if (!grown) {
			break;
		}
		buffer = grown;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
		len = strftime(buffer, cap, fmt, &tm_info);
#pragma GCC diagnostic pop
		if (len > 0 || fmt[0] == '\0') {
			break;
		}
	}
	if (!buffer || (len == 0 && fmt[0] != '\0')) {
		free(buffer);
		const int code = qd_fail(ctx, TIME_ERR_FORMAT, "time::format: cannot format \"%s\"", fmt);
		qd_string_release(format);
		return code;
	}
	qd_string_release(format);
	qd_string_t* result = qd_string_create_with_length(buffer, len);
	free(buffer);
	qd_push_s_ref(ctx, result);
	qd_string_release(result);
	return qd_push_i(ctx, TIME_ERR_OK);
}

// Stack signature: ( input:str format:str -- timestamp:i64 )!
// The whole input must match the format: "2024-01-15xyz" read as 2024-01-15.
int usr_time_parse(qd_context* ctx) {
	qd_string_t* input;
	qd_string_t* format;
	qd_args(ctx, "time::parse", "ss", &input, &format);

	struct tm tm_info;
	memset(&tm_info, 0, sizeof(tm_info));
	const char* rest = strptime(qd_string_data(input), qd_string_data(format), &tm_info);
	bool ok = rest != NULL && *rest == '\0';
	time_t timestamp = 0;
	if (ok) {
		// -1 is also the second before the epoch, which timegm returns for it like any other;
		// it means failure only when the fields it normalised say another time
		const struct tm asked = tm_info;
		timestamp = timegm(&tm_info);
		ok = timestamp != (time_t)-1 || (asked.tm_year == 69 && asked.tm_mon == 11 && asked.tm_mday == 31 &&
												asked.tm_hour == 23 && asked.tm_min == 59 && asked.tm_sec == 59);
	}
	if (!ok) {
		const int code = qd_fail(ctx, TIME_ERR_PARSE, "time::parse: \"%s\" does not match \"%s\"",
				qd_string_data(input), qd_string_data(format));
		qd_string_release(input);
		qd_string_release(format);
		return code;
	}
	qd_string_release(input);
	qd_string_release(format);
	qd_push_i(ctx, (int64_t)timestamp);
	return qd_push_i(ctx, TIME_ERR_OK);
}
