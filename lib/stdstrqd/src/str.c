#define _POSIX_C_SOURCE 200809L

#include <stdstrqd/str.h>
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

// substring - extract substring ( str:s start:i length:i -- result:s )!
qd_exec_result usr_str_substring(qd_context* ctx) {
	qd_stack_element_t len_elem, start_elem, str_elem;

	// Pop length
	qd_stack_error err = qd_stack_pop(ctx->st, &len_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::substring: Stack underflow\n");
		abort();
	}

	// Pop start
	err = qd_stack_pop(ctx->st, &start_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::substring: Stack underflow\n");
		abort();
	}

	// Pop string
	err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::substring: Stack underflow\n");
		abort();
	}

	if (str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::substring: Expected string\n");
		abort();
	}

	if (start_elem.type != QD_STACK_TYPE_INT || len_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in str::substring: Expected integers for start and length\n");
		free(str_elem.value.s);
		abort();
	}

	int64_t start = start_elem.value.i;
	int64_t length = len_elem.value.i;
	size_t str_len = strlen(str_elem.value.s);

	if (start < 0 || length < 0) {
		fprintf(stderr, "Fatal error in str::substring: Negative indices not allowed\n");
		free(str_elem.value.s);
		abort();
	}

	if ((size_t)start > str_len) {
		fprintf(stderr, "Fatal error in str::substring: Start index out of bounds\n");
		free(str_elem.value.s);
		abort();
	}

	// Adjust length to not exceed string bounds
	size_t actual_length = (size_t)length;
	if ((size_t)start + actual_length > str_len) {
		actual_length = str_len - (size_t)start;
	}

	char* result = malloc(actual_length + 1);
	if (!result) {
		fprintf(stderr, "Fatal error in str::substring: Memory allocation failed\n");
		free(str_elem.value.s);
		abort();
	}

	strncpy(result, str_elem.value.s + start, actual_length);
	result[actual_length] = '\0';

	free(str_elem.value.s);
	qd_push_s(ctx, result);
	free(result);

	return (qd_exec_result){0};
}

// split - split string by delimiter ( str:s delim:s -- parts:p count:i )
// Returns pointer to array of strings and count
qd_exec_result usr_str_split(qd_context* ctx) {
	qd_stack_element_t delim_elem, str_elem;

	// Pop delimiter
	qd_stack_error err = qd_stack_pop(ctx->st, &delim_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::split: Stack underflow\n");
		abort();
	}

	// Pop string
	err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::split: Stack underflow\n");
		if (delim_elem.type == QD_STACK_TYPE_STR) free(delim_elem.value.s);
		abort();
	}

	if (str_elem.type != QD_STACK_TYPE_STR || delim_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::split: Expected two strings\n");
		if (str_elem.type == QD_STACK_TYPE_STR) free(str_elem.value.s);
		if (delim_elem.type == QD_STACK_TYPE_STR) free(delim_elem.value.s);
		abort();
	}

	const char* delim = delim_elem.value.s;
	size_t delim_len = strlen(delim);

	if (delim_len == 0) {
		fprintf(stderr, "Fatal error in str::split: Empty delimiter\n");
		free(str_elem.value.s);
		free(delim_elem.value.s);
		abort();
	}

	// Count parts
	size_t count = 1;
	const char* pos = str_elem.value.s;
	while ((pos = strstr(pos, delim)) != NULL) {
		count++;
		pos += delim_len;
	}

	// Allocate array for parts
	char** parts = malloc(count * sizeof(char*));
	if (!parts) {
		fprintf(stderr, "Fatal error in str::split: Memory allocation failed\n");
		free(str_elem.value.s);
		free(delim_elem.value.s);
		abort();
	}

	// Split string
	size_t idx = 0;
	const char* start = str_elem.value.s;
	pos = str_elem.value.s;

	while ((pos = strstr(pos, delim)) != NULL) {
		size_t part_len = (size_t)(pos - start);
		parts[idx] = malloc(part_len + 1);
		if (!parts[idx]) {
			fprintf(stderr, "Fatal error in str::split: Memory allocation failed\n");
			for (size_t i = 0; i < idx; i++) free(parts[i]);
			free(parts);
			free(str_elem.value.s);
			free(delim_elem.value.s);
			abort();
		}
		strncpy(parts[idx], start, part_len);
		parts[idx][part_len] = '\0';
		idx++;
		pos += delim_len;
		start = pos;
	}

	// Last part
	size_t part_len = strlen(start);
	parts[idx] = malloc(part_len + 1);
	if (!parts[idx]) {
		fprintf(stderr, "Fatal error in str::split: Memory allocation failed\n");
		for (size_t i = 0; i < idx; i++) free(parts[i]);
		free(parts);
		free(str_elem.value.s);
		free(delim_elem.value.s);
		abort();
	}
	strcpy(parts[idx], start);

	free(str_elem.value.s);
	free(delim_elem.value.s);

	qd_push_p(ctx, parts);
	qd_push_i(ctx, (int64_t)count);

	return (qd_exec_result){0};
}

// replace - replace all occurrences ( str:s old:s new:s -- result:s )
qd_exec_result usr_str_replace(qd_context* ctx) {
	qd_stack_element_t new_elem, old_elem, str_elem;

	// Pop new
	qd_stack_error err = qd_stack_pop(ctx->st, &new_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::replace: Stack underflow\n");
		abort();
	}

	// Pop old
	err = qd_stack_pop(ctx->st, &old_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::replace: Stack underflow\n");
		if (new_elem.type == QD_STACK_TYPE_STR) free(new_elem.value.s);
		abort();
	}

	// Pop str
	err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::replace: Stack underflow\n");
		if (new_elem.type == QD_STACK_TYPE_STR) free(new_elem.value.s);
		if (old_elem.type == QD_STACK_TYPE_STR) free(old_elem.value.s);
		abort();
	}

	if (str_elem.type != QD_STACK_TYPE_STR || old_elem.type != QD_STACK_TYPE_STR ||
			new_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::replace: Expected three strings\n");
		if (str_elem.type == QD_STACK_TYPE_STR) free(str_elem.value.s);
		if (old_elem.type == QD_STACK_TYPE_STR) free(old_elem.value.s);
		if (new_elem.type == QD_STACK_TYPE_STR) free(new_elem.value.s);
		abort();
	}

	const char* old = old_elem.value.s;
	const char* new = new_elem.value.s;
	size_t old_len = strlen(old);
	size_t new_len = strlen(new);

	if (old_len == 0) {
		// Can't replace empty string, return original
		free(old_elem.value.s);
		free(new_elem.value.s);
		qd_push_s(ctx, str_elem.value.s);
		free(str_elem.value.s);
		return (qd_exec_result){0};
	}

	// Count occurrences
	size_t count = 0;
	const char* pos = str_elem.value.s;
	while ((pos = strstr(pos, old)) != NULL) {
		count++;
		pos += old_len;
	}

	// Calculate result length
	size_t str_len = strlen(str_elem.value.s);
	size_t result_len = str_len + count * (new_len - old_len);

	char* result = malloc(result_len + 1);
	if (!result) {
		fprintf(stderr, "Fatal error in str::replace: Memory allocation failed\n");
		free(str_elem.value.s);
		free(old_elem.value.s);
		free(new_elem.value.s);
		abort();
	}

	// Build result string
	char* dest = result;
	const char* src = str_elem.value.s;
	pos = str_elem.value.s;

	while ((pos = strstr(pos, old)) != NULL) {
		// Copy up to match
		size_t prefix_len = (size_t)(pos - src);
		strncpy(dest, src, prefix_len);
		dest += prefix_len;

		// Copy replacement
		strcpy(dest, new);
		dest += new_len;

		// Move past match
		pos += old_len;
		src = pos;
	}

	// Copy remaining
	strcpy(dest, src);

	free(str_elem.value.s);
	free(old_elem.value.s);
	free(new_elem.value.s);

	qd_push_s(ctx, result);
	free(result);

	return (qd_exec_result){0};
}

// compare - compare two strings ( str1:s str2:s -- result:i )
// Returns: -1 if str1 < str2, 0 if equal, 1 if str1 > str2
qd_exec_result usr_str_compare(qd_context* ctx) {
	qd_stack_element_t str2_elem, str1_elem;

	// Pop str2
	qd_stack_error err = qd_stack_pop(ctx->st, &str2_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::compare: Stack underflow\n");
		abort();
	}

	// Pop str1
	err = qd_stack_pop(ctx->st, &str1_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::compare: Stack underflow\n");
		if (str2_elem.type == QD_STACK_TYPE_STR) free(str2_elem.value.s);
		abort();
	}

	if (str1_elem.type != QD_STACK_TYPE_STR || str2_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::compare: Expected two strings\n");
		if (str1_elem.type == QD_STACK_TYPE_STR) free(str1_elem.value.s);
		if (str2_elem.type == QD_STACK_TYPE_STR) free(str2_elem.value.s);
		abort();
	}

	int cmp = strcmp(str1_elem.value.s, str2_elem.value.s);
	int result = (cmp < 0) ? -1 : (cmp > 0) ? 1 : 0;

	free(str1_elem.value.s);
	free(str2_elem.value.s);

	qd_push_i(ctx, result);
	return (qd_exec_result){0};
}
