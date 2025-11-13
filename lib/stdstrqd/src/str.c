#define _POSIX_C_SOURCE 200809L

#include <stdqd/str.h>
#include <qdrt/stack.h>
#include <qdrt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// len - get string length ( str:s -- len:i )
qd_exec_result usr_str_len(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::len: Stack underflow\n");
		abort();
	}

	if (val.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::len: Expected string, got type %d\n", val.type);
		abort();
	}

	size_t len = strlen(val.value.s);
	free(val.value.s);

	qd_push_i(ctx, (int64_t)len);
	return (qd_exec_result){0};
}

// concat - concatenate two strings ( str1:s str2:s -- result:s )
qd_exec_result usr_str_concat(qd_context* ctx) {
	qd_stack_element_t str2, str1;

	// Pop str2 (top)
	qd_stack_error err = qd_stack_pop(ctx->st, &str2);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::concat: Stack underflow\n");
		abort();
	}

	// Pop str1
	err = qd_stack_pop(ctx->st, &str1);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::concat: Stack underflow\n");
		if (str2.type == QD_STACK_TYPE_STR) free(str2.value.s);
		abort();
	}

	if (str1.type != QD_STACK_TYPE_STR || str2.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::concat: Expected two strings\n");
		if (str1.type == QD_STACK_TYPE_STR) free(str1.value.s);
		if (str2.type == QD_STACK_TYPE_STR) free(str2.value.s);
		abort();
	}

	size_t len1 = strlen(str1.value.s);
	size_t len2 = strlen(str2.value.s);
	char* result = malloc(len1 + len2 + 1);

	if (!result) {
		fprintf(stderr, "Fatal error in str::concat: Memory allocation failed\n");
		free(str1.value.s);
		free(str2.value.s);
		abort();
	}

	strcpy(result, str1.value.s);
	strcat(result, str2.value.s);

	free(str1.value.s);
	free(str2.value.s);

	qd_push_s(ctx, result);
	free(result);

	return (qd_exec_result){0};
}

// contains - check if string contains substring ( str:s needle:s -- contains:i )
qd_exec_result usr_str_contains(qd_context* ctx) {
	qd_stack_element_t needle, haystack;

	// Pop needle (top)
	qd_stack_error err = qd_stack_pop(ctx->st, &needle);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::contains: Stack underflow\n");
		abort();
	}

	// Pop haystack
	err = qd_stack_pop(ctx->st, &haystack);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::contains: Stack underflow\n");
		if (needle.type == QD_STACK_TYPE_STR) free(needle.value.s);
		abort();
	}

	if (haystack.type != QD_STACK_TYPE_STR || needle.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::contains: Expected two strings\n");
		if (haystack.type == QD_STACK_TYPE_STR) free(haystack.value.s);
		if (needle.type == QD_STACK_TYPE_STR) free(needle.value.s);
		abort();
	}

	int result = (strstr(haystack.value.s, needle.value.s) != NULL) ? 1 : 0;

	free(haystack.value.s);
	free(needle.value.s);

	qd_push_i(ctx, result);
	return (qd_exec_result){0};
}

// starts_with - check if string starts with prefix ( str:s prefix:s -- result:i )
qd_exec_result usr_str_starts_with(qd_context* ctx) {
	qd_stack_element_t prefix, str;

	// Pop prefix (top)
	qd_stack_error err = qd_stack_pop(ctx->st, &prefix);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::starts_with: Stack underflow\n");
		abort();
	}

	// Pop str
	err = qd_stack_pop(ctx->st, &str);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::starts_with: Stack underflow\n");
		if (prefix.type == QD_STACK_TYPE_STR) free(prefix.value.s);
		abort();
	}

	if (str.type != QD_STACK_TYPE_STR || prefix.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::starts_with: Expected two strings\n");
		if (str.type == QD_STACK_TYPE_STR) free(str.value.s);
		if (prefix.type == QD_STACK_TYPE_STR) free(prefix.value.s);
		abort();
	}

	size_t str_len = strlen(str.value.s);
	size_t prefix_len = strlen(prefix.value.s);

	int result = 0;
	if (prefix_len <= str_len) {
		result = (strncmp(str.value.s, prefix.value.s, prefix_len) == 0) ? 1 : 0;
	}

	free(str.value.s);
	free(prefix.value.s);

	qd_push_i(ctx, result);
	return (qd_exec_result){0};
}

// ends_with - check if string ends with suffix ( str:s suffix:s -- result:i )
qd_exec_result usr_str_ends_with(qd_context* ctx) {
	qd_stack_element_t suffix, str;

	// Pop suffix (top)
	qd_stack_error err = qd_stack_pop(ctx->st, &suffix);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::ends_with: Stack underflow\n");
		abort();
	}

	// Pop str
	err = qd_stack_pop(ctx->st, &str);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::ends_with: Stack underflow\n");
		if (suffix.type == QD_STACK_TYPE_STR) free(suffix.value.s);
		abort();
	}

	if (str.type != QD_STACK_TYPE_STR || suffix.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::ends_with: Expected two strings\n");
		if (str.type == QD_STACK_TYPE_STR) free(str.value.s);
		if (suffix.type == QD_STACK_TYPE_STR) free(suffix.value.s);
		abort();
	}

	size_t str_len = strlen(str.value.s);
	size_t suffix_len = strlen(suffix.value.s);

	int result = 0;
	if (suffix_len <= str_len) {
		const char* str_end = str.value.s + (str_len - suffix_len);
		result = (strcmp(str_end, suffix.value.s) == 0) ? 1 : 0;
	}

	free(str.value.s);
	free(suffix.value.s);

	qd_push_i(ctx, result);
	return (qd_exec_result){0};
}

// upper - convert string to uppercase ( str:s -- result:s )
qd_exec_result usr_str_upper(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::upper: Stack underflow\n");
		abort();
	}

	if (val.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::upper: Expected string, got type %d\n", val.type);
		abort();
	}

	size_t len = strlen(val.value.s);
	char* result = malloc(len + 1);

	if (!result) {
		fprintf(stderr, "Fatal error in str::upper: Memory allocation failed\n");
		free(val.value.s);
		abort();
	}

	for (size_t i = 0; i <= len; i++) {
		result[i] = (char)toupper((unsigned char)val.value.s[i]);
	}

	free(val.value.s);
	qd_push_s(ctx, result);
	free(result);

	return (qd_exec_result){0};
}

// lower - convert string to lowercase ( str:s -- result:s )
qd_exec_result usr_str_lower(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::lower: Stack underflow\n");
		abort();
	}

	if (val.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::lower: Expected string, got type %d\n", val.type);
		abort();
	}

	size_t len = strlen(val.value.s);
	char* result = malloc(len + 1);

	if (!result) {
		fprintf(stderr, "Fatal error in str::lower: Memory allocation failed\n");
		free(val.value.s);
		abort();
	}

	for (size_t i = 0; i <= len; i++) {
		result[i] = (char)tolower((unsigned char)val.value.s[i]);
	}

	free(val.value.s);
	qd_push_s(ctx, result);
	free(result);

	return (qd_exec_result){0};
}

// trim - remove leading and trailing whitespace ( str:s -- result:s )
qd_exec_result usr_str_trim(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::trim: Stack underflow\n");
		abort();
	}

	if (val.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::trim: Expected string, got type %d\n", val.type);
		abort();
	}

	const char* start = val.value.s;
	const char* end = val.value.s + strlen(val.value.s);

	// Trim leading whitespace
	while (*start && isspace((unsigned char)*start)) {
		start++;
	}

	// Trim trailing whitespace
	if (end > start) {
		end--;
		while (end > start && isspace((unsigned char)*end)) {
			end--;
		}
	}

	// Calculate length
	size_t trimmed_len;
	if (end >= start) {
		trimmed_len = (size_t)(end - start) + 1;
	} else {
		// String is all whitespace
		trimmed_len = 0;
	}

	char* result = malloc(trimmed_len + 1);

	if (!result) {
		fprintf(stderr, "Fatal error in str::trim: Memory allocation failed\n");
		free(val.value.s);
		abort();
	}

	if (trimmed_len > 0) {
		strncpy(result, start, trimmed_len);
	}
	result[trimmed_len] = '\0';

	free(val.value.s);
	qd_push_s(ctx, result);
	free(result);

	return (qd_exec_result){0};
}
