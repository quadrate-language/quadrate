#define _POSIX_C_SOURCE 200809L

#include <qdstr/str.h>
#include <qdrt/stack.h>
#include <qdrt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Error codes matching module.qd
#define STR_ERR_OK 1            // Success (matches builtin Ok)
#define STR_ERR_OUT_OF_BOUNDS 2 // Index out of bounds
#define STR_ERR_ALLOC 3         // Allocation failed
#define STR_ERR_INVALID_ARG 4   // Invalid argument

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

	size_t len = strlen(qd_string_data(val.value.s));
	qd_string_release(val.value.s);

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
		if (str2.type == QD_STACK_TYPE_STR) qd_string_release(str2.value.s);
		abort();
	}

	if (str1.type != QD_STACK_TYPE_STR || str2.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::concat: Expected two strings\n");
		if (str1.type == QD_STACK_TYPE_STR) qd_string_release(str1.value.s);
		if (str2.type == QD_STACK_TYPE_STR) qd_string_release(str2.value.s);
		abort();
	}

	size_t len1 = strlen(qd_string_data(str1.value.s));
	size_t len2 = strlen(qd_string_data(str2.value.s));
	char* result = malloc(len1 + len2 + 1);

	if (!result) {
		fprintf(stderr, "Fatal error in str::concat: Memory allocation failed\n");
		qd_string_release(str1.value.s);
		qd_string_release(str2.value.s);
		abort();
	}

	memcpy(result, qd_string_data(str1.value.s), len1);
	memcpy(result + len1, qd_string_data(str2.value.s), len2 + 1);  // +1 for null terminator

	qd_string_release(str1.value.s);
	qd_string_release(str2.value.s);

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
		if (needle.type == QD_STACK_TYPE_STR) qd_string_release(needle.value.s);
		abort();
	}

	if (haystack.type != QD_STACK_TYPE_STR || needle.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::contains: Expected two strings\n");
		if (haystack.type == QD_STACK_TYPE_STR) qd_string_release(haystack.value.s);
		if (needle.type == QD_STACK_TYPE_STR) qd_string_release(needle.value.s);
		abort();
	}

	int result = (strstr(qd_string_data(haystack.value.s), qd_string_data(needle.value.s)) != NULL) ? 1 : 0;

	qd_string_release(haystack.value.s);
	qd_string_release(needle.value.s);

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
		if (prefix.type == QD_STACK_TYPE_STR) qd_string_release(prefix.value.s);
		abort();
	}

	if (str.type != QD_STACK_TYPE_STR || prefix.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::starts_with: Expected two strings\n");
		if (str.type == QD_STACK_TYPE_STR) qd_string_release(str.value.s);
		if (prefix.type == QD_STACK_TYPE_STR) qd_string_release(prefix.value.s);
		abort();
	}

	size_t str_len = strlen(qd_string_data(str.value.s));
	size_t prefix_len = strlen(qd_string_data(prefix.value.s));

	int result = 0;
	if (prefix_len <= str_len) {
		result = (strncmp(qd_string_data(str.value.s), qd_string_data(prefix.value.s), prefix_len) == 0) ? 1 : 0;
	}

	qd_string_release(str.value.s);
	qd_string_release(prefix.value.s);

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
		if (suffix.type == QD_STACK_TYPE_STR) qd_string_release(suffix.value.s);
		abort();
	}

	if (str.type != QD_STACK_TYPE_STR || suffix.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::ends_with: Expected two strings\n");
		if (str.type == QD_STACK_TYPE_STR) qd_string_release(str.value.s);
		if (suffix.type == QD_STACK_TYPE_STR) qd_string_release(suffix.value.s);
		abort();
	}

	size_t str_len = strlen(qd_string_data(str.value.s));
	size_t suffix_len = strlen(qd_string_data(suffix.value.s));

	int result = 0;
	if (suffix_len <= str_len) {
		const char* str_end = qd_string_data(str.value.s) + (str_len - suffix_len);
		result = (strcmp(str_end, qd_string_data(suffix.value.s)) == 0) ? 1 : 0;
	}

	qd_string_release(str.value.s);
	qd_string_release(suffix.value.s);

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

	size_t len = strlen(qd_string_data(val.value.s));
	char* result = malloc(len + 1);

	if (!result) {
		fprintf(stderr, "Fatal error in str::upper: Memory allocation failed\n");
		qd_string_release(val.value.s);
		abort();
	}

	for (size_t i = 0; i <= len; i++) {
		result[i] = (char)toupper((unsigned char)qd_string_data(val.value.s)[i]);
	}

	qd_string_release(val.value.s);
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

	size_t len = strlen(qd_string_data(val.value.s));
	char* result = malloc(len + 1);

	if (!result) {
		fprintf(stderr, "Fatal error in str::lower: Memory allocation failed\n");
		qd_string_release(val.value.s);
		abort();
	}

	for (size_t i = 0; i <= len; i++) {
		result[i] = (char)tolower((unsigned char)qd_string_data(val.value.s)[i]);
	}

	qd_string_release(val.value.s);
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

	const char* start = qd_string_data(val.value.s);
	const char* end = start + strlen(start);

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
		qd_string_release(val.value.s);
		abort();
	}

	if (trimmed_len > 0) {
		memcpy(result, start, trimmed_len);
	}
	result[trimmed_len] = '\0';

	qd_string_release(val.value.s);
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
		qd_string_release(str_elem.value.s);
		abort();
	}

	int64_t start = start_elem.value.i;
	int64_t length = len_elem.value.i;
	size_t str_len = strlen(qd_string_data(str_elem.value.s));

	if (start < 0 || length < 0) {
		fprintf(stderr, "Fatal error in str::substring: Negative indices not allowed\n");
		qd_string_release(str_elem.value.s);
		abort();
	}

	if ((size_t)start > str_len) {
		fprintf(stderr, "Fatal error in str::substring: Start index out of bounds\n");
		qd_string_release(str_elem.value.s);
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
		qd_string_release(str_elem.value.s);
		abort();
	}

	memcpy(result, qd_string_data(str_elem.value.s) + start, actual_length);
	result[actual_length] = '\0';

	qd_string_release(str_elem.value.s);
	qd_push_s(ctx, result);
	free(result);
	qd_push_i(ctx, STR_ERR_OK);  // Success status for fallible function

	return (qd_exec_result){STR_ERR_OK};
}

// split - split string by delimiter ( str:s delim:s -- parts:p count:i )
// Returns pointer to array of qd_string* and count
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
		if (delim_elem.type == QD_STACK_TYPE_STR) qd_string_release(delim_elem.value.s);
		abort();
	}

	if (str_elem.type != QD_STACK_TYPE_STR || delim_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::split: Expected two strings\n");
		if (str_elem.type == QD_STACK_TYPE_STR) qd_string_release(str_elem.value.s);
		if (delim_elem.type == QD_STACK_TYPE_STR) qd_string_release(delim_elem.value.s);
		abort();
	}

	const char* delim = qd_string_data(delim_elem.value.s);
	size_t delim_len = strlen(delim);

	if (delim_len == 0) {
		fprintf(stderr, "Fatal error in str::split: Empty delimiter\n");
		qd_string_release(str_elem.value.s);
		qd_string_release(delim_elem.value.s);
		abort();
	}

	// Count parts
	size_t count = 1;
	const char* pos = qd_string_data(str_elem.value.s);
	while ((pos = strstr(pos, delim)) != NULL) {
		count++;
		pos += delim_len;
	}

	// Allocate array for qd_string pointers
	qd_string_t** parts = malloc(count * sizeof(qd_string_t*));
	if (!parts) {
		fprintf(stderr, "Fatal error in str::split: Memory allocation failed\n");
		qd_string_release(str_elem.value.s);
		qd_string_release(delim_elem.value.s);
		abort();
	}

	// Split string
	size_t idx = 0;
	const char* start = qd_string_data(str_elem.value.s);
	pos = qd_string_data(str_elem.value.s);

	while ((pos = strstr(pos, delim)) != NULL) {
		size_t part_len = (size_t)(pos - start);
		parts[idx] = qd_string_create_with_length(start, part_len);
		if (!parts[idx]) {
			fprintf(stderr, "Fatal error in str::split: Memory allocation failed\n");
			for (size_t i = 0; i < idx; i++) qd_string_release(parts[i]);
			free(parts);
			qd_string_release(str_elem.value.s);
			qd_string_release(delim_elem.value.s);
			abort();
		}
		idx++;
		pos += delim_len;
		start = pos;
	}

	// Last part
	size_t part_len = strlen(start);
	parts[idx] = qd_string_create_with_length(start, part_len);
	if (!parts[idx]) {
		fprintf(stderr, "Fatal error in str::split: Memory allocation failed\n");
		for (size_t i = 0; i < idx; i++) qd_string_release(parts[i]);
		free(parts);
		qd_string_release(str_elem.value.s);
		qd_string_release(delim_elem.value.s);
		abort();
	}

	qd_string_release(str_elem.value.s);
	qd_string_release(delim_elem.value.s);

	qd_push_p(ctx, parts);
	qd_push_i(ctx, (int64_t)count);
	qd_push_i(ctx, STR_ERR_OK);  // Success status for fallible function

	return (qd_exec_result){STR_ERR_OK};
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
		if (new_elem.type == QD_STACK_TYPE_STR) qd_string_release(new_elem.value.s);
		abort();
	}

	// Pop str
	err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::replace: Stack underflow\n");
		if (new_elem.type == QD_STACK_TYPE_STR) qd_string_release(new_elem.value.s);
		if (old_elem.type == QD_STACK_TYPE_STR) qd_string_release(old_elem.value.s);
		abort();
	}

	if (str_elem.type != QD_STACK_TYPE_STR || old_elem.type != QD_STACK_TYPE_STR ||
			new_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::replace: Expected three strings\n");
		if (str_elem.type == QD_STACK_TYPE_STR) qd_string_release(str_elem.value.s);
		if (old_elem.type == QD_STACK_TYPE_STR) qd_string_release(old_elem.value.s);
		if (new_elem.type == QD_STACK_TYPE_STR) qd_string_release(new_elem.value.s);
		abort();
	}

	const char* old = qd_string_data(old_elem.value.s);
	const char* new = qd_string_data(new_elem.value.s);
	size_t old_len = strlen(old);
	size_t new_len = strlen(new);

	if (old_len == 0) {
		// Can't replace empty string, return original
		qd_string_release(old_elem.value.s);
		qd_string_release(new_elem.value.s);
		qd_push_s(ctx, qd_string_data(str_elem.value.s));
		qd_string_release(str_elem.value.s);
		qd_push_i(ctx, STR_ERR_OK);  // Success status for fallible function
		return (qd_exec_result){STR_ERR_OK};
	}

	// Count occurrences
	size_t count = 0;
	const char* pos = qd_string_data(str_elem.value.s);
	while ((pos = strstr(pos, old)) != NULL) {
		count++;
		pos += old_len;
	}

	// Calculate result length
	size_t str_len = strlen(qd_string_data(str_elem.value.s));
	size_t result_len = str_len + count * (new_len - old_len);

	char* result = malloc(result_len + 1);
	if (!result) {
		fprintf(stderr, "Fatal error in str::replace: Memory allocation failed\n");
		qd_string_release(str_elem.value.s);
		qd_string_release(old_elem.value.s);
		qd_string_release(new_elem.value.s);
		abort();
	}

	// Build result string
	char* dest = result;
	const char* src = qd_string_data(str_elem.value.s);
	pos = qd_string_data(str_elem.value.s);

	while ((pos = strstr(pos, old)) != NULL) {
		// Copy up to match
		size_t prefix_len = (size_t)(pos - src);
		memcpy(dest, src, prefix_len);
		dest += prefix_len;

		// Copy replacement
		memcpy(dest, new, new_len);
		dest += new_len;

		// Move past match
		pos += old_len;
		src = pos;
	}

	// Copy remaining (including null terminator)
	size_t remaining = strlen(src);
	memcpy(dest, src, remaining + 1);

	qd_string_release(str_elem.value.s);
	qd_string_release(old_elem.value.s);
	qd_string_release(new_elem.value.s);

	qd_push_s(ctx, result);
	free(result);
	qd_push_i(ctx, STR_ERR_OK);  // Success status for fallible function

	return (qd_exec_result){STR_ERR_OK};
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
		if (str2_elem.type == QD_STACK_TYPE_STR) qd_string_release(str2_elem.value.s);
		abort();
	}

	if (str1_elem.type != QD_STACK_TYPE_STR || str2_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::compare: Expected two strings\n");
		if (str1_elem.type == QD_STACK_TYPE_STR) qd_string_release(str1_elem.value.s);
		if (str2_elem.type == QD_STACK_TYPE_STR) qd_string_release(str2_elem.value.s);
		abort();
	}

	int cmp = strcmp(qd_string_data(str1_elem.value.s), qd_string_data(str2_elem.value.s));
	int result = (cmp < 0) ? -1 : (cmp > 0) ? 1 : 0;

	qd_string_release(str1_elem.value.s);
	qd_string_release(str2_elem.value.s);

	qd_push_i(ctx, result);
	return (qd_exec_result){0};
}

// char_at - get character code at index ( str:s index:i -- char_code:i )!
qd_exec_result usr_str_char_at(qd_context* ctx) {
	qd_stack_element_t index_elem, str_elem;

	// Pop index
	qd_stack_error err = qd_stack_pop(ctx->st, &index_elem);
	if (err != QD_STACK_OK) {
		qd_push_i(ctx, STR_ERR_INVALID_ARG);
		return (qd_exec_result){STR_ERR_INVALID_ARG};
	}

	// Pop string
	err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		qd_push_i(ctx, STR_ERR_INVALID_ARG);
		return (qd_exec_result){STR_ERR_INVALID_ARG};
	}

	if (str_elem.type != QD_STACK_TYPE_STR) {
		qd_push_i(ctx, STR_ERR_INVALID_ARG);
		return (qd_exec_result){STR_ERR_INVALID_ARG};
	}

	if (index_elem.type != QD_STACK_TYPE_INT) {
		qd_string_release(str_elem.value.s);
		qd_push_i(ctx, STR_ERR_INVALID_ARG);
		return (qd_exec_result){STR_ERR_INVALID_ARG};
	}

	int64_t index = index_elem.value.i;
	size_t str_len = strlen(qd_string_data(str_elem.value.s));

	if (index < 0 || (size_t)index >= str_len) {
		qd_string_release(str_elem.value.s);
		qd_push_i(ctx, STR_ERR_OUT_OF_BOUNDS);
		return (qd_exec_result){STR_ERR_OUT_OF_BOUNDS};
	}

	int64_t char_code = (unsigned char)qd_string_data(str_elem.value.s)[index];

	qd_string_release(str_elem.value.s);

	// Push result then Ok
	qd_push_i(ctx, char_code);
	qd_push_i(ctx, STR_ERR_OK);
	return (qd_exec_result){0};
}

// index_of - find index of substring ( haystack:s needle:s -- index:i )
qd_exec_result usr_str_index_of(qd_context* ctx) {
	qd_stack_element_t needle_elem, haystack_elem;

	// Pop needle
	qd_stack_error err = qd_stack_pop(ctx->st, &needle_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::index_of: Stack underflow\n");
		abort();
	}

	// Pop haystack
	err = qd_stack_pop(ctx->st, &haystack_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::index_of: Stack underflow\n");
		if (needle_elem.type == QD_STACK_TYPE_STR) qd_string_release(needle_elem.value.s);
		abort();
	}

	if (haystack_elem.type != QD_STACK_TYPE_STR || needle_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::index_of: Expected two strings\n");
		if (haystack_elem.type == QD_STACK_TYPE_STR) qd_string_release(haystack_elem.value.s);
		if (needle_elem.type == QD_STACK_TYPE_STR) qd_string_release(needle_elem.value.s);
		abort();
	}

	const char* haystack = qd_string_data(haystack_elem.value.s);
	const char* needle = qd_string_data(needle_elem.value.s);
	const char* pos = strstr(haystack, needle);

	int64_t result = (pos != NULL) ? (int64_t)(pos - haystack) : -1;

	qd_string_release(haystack_elem.value.s);
	qd_string_release(needle_elem.value.s);

	qd_push_i(ctx, result);
	return (qd_exec_result){0};
}

// index_of_from - find index of substring starting from position ( haystack:s needle:s start:i -- index:i )
qd_exec_result usr_str_index_of_from(qd_context* ctx) {
	qd_stack_element_t start_elem, needle_elem, haystack_elem;

	// Pop start
	qd_stack_error err = qd_stack_pop(ctx->st, &start_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::index_of_from: Stack underflow\n");
		abort();
	}

	// Pop needle
	err = qd_stack_pop(ctx->st, &needle_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::index_of_from: Stack underflow\n");
		abort();
	}

	// Pop haystack
	err = qd_stack_pop(ctx->st, &haystack_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::index_of_from: Stack underflow\n");
		if (needle_elem.type == QD_STACK_TYPE_STR) qd_string_release(needle_elem.value.s);
		abort();
	}

	if (haystack_elem.type != QD_STACK_TYPE_STR || needle_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::index_of_from: Expected two strings\n");
		if (haystack_elem.type == QD_STACK_TYPE_STR) qd_string_release(haystack_elem.value.s);
		if (needle_elem.type == QD_STACK_TYPE_STR) qd_string_release(needle_elem.value.s);
		abort();
	}

	if (start_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in str::index_of_from: Expected integer start position\n");
		qd_string_release(haystack_elem.value.s);
		qd_string_release(needle_elem.value.s);
		abort();
	}

	const char* haystack = qd_string_data(haystack_elem.value.s);
	const char* needle = qd_string_data(needle_elem.value.s);
	int64_t start = start_elem.value.i;
	size_t haystack_len = strlen(haystack);

	int64_t result = -1;
	if (start >= 0 && (size_t)start < haystack_len) {
		const char* pos = strstr(haystack + start, needle);
		if (pos != NULL) {
			result = (int64_t)(pos - haystack);
		}
	}

	qd_string_release(haystack_elem.value.s);
	qd_string_release(needle_elem.value.s);

	qd_push_i(ctx, result);
	return (qd_exec_result){0};
}

// from_char - create string from character code ( char_code:i -- str:s )
qd_exec_result usr_str_from_char(qd_context* ctx) {
	qd_stack_element_t code_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &code_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::from_char: Stack underflow\n");
		abort();
	}

	if (code_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in str::from_char: Expected integer\n");
		abort();
	}

	int64_t char_code = code_elem.value.i;

	// Handle single-byte ASCII characters
	if (char_code >= 0 && char_code <= 127) {
		char result[2];
		result[0] = (char)char_code;
		result[1] = '\0';
		qd_push_s(ctx, result);
	} else {
		// For non-ASCII, we could support UTF-8 encoding here
		// For now, just handle single-byte values
		char result[2];
		result[0] = (char)(char_code & 0xFF);
		result[1] = '\0';
		qd_push_s(ctx, result);
	}

	return (qd_exec_result){0};
}

// from_ptr - convert C string pointer to Quadrate string ( ptr:p -- str:s )
qd_exec_result usr_str_from_ptr(qd_context* ctx) {
	qd_stack_element_t ptr_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &ptr_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in str::from_ptr: Stack underflow\n");
		abort();
	}

	if (ptr_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in str::from_ptr: Expected pointer\n");
		abort();
	}

	const char* c_str = (const char*)ptr_elem.value.p;
	if (c_str == NULL) {
		qd_push_s(ctx, "");
	} else {
		qd_push_s(ctx, c_str);
	}

	return (qd_exec_result){0};
}

// Comparison function for qsort (ascending)
static int str_cmp_asc(const void* a, const void* b) {
	const char* str_a = *(const char**)a;
	const char* str_b = *(const char**)b;
	return strcmp(str_a, str_b);
}

// Comparison function for qsort (descending)
static int str_cmp_desc(const void* a, const void* b) {
	const char* str_a = *(const char**)a;
	const char* str_b = *(const char**)b;
	return strcmp(str_b, str_a);
}

// sort - sort array of strings in ascending order ( arr:p count:i -- )
qd_exec_result usr_str_sort(qd_context* ctx) {
	qd_stack_element_t count_elem, arr_elem;

	// Pop count
	qd_stack_error err = qd_stack_pop(ctx->st, &count_elem);
	if (err != QD_STACK_OK || count_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in str::sort: Expected integer count\n");
		abort();
	}

	// Pop array pointer
	err = qd_stack_pop(ctx->st, &arr_elem);
	if (err != QD_STACK_OK || arr_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in str::sort: Expected pointer to string array\n");
		abort();
	}

	int64_t count = count_elem.value.i;
	char** arr = (char**)arr_elem.value.p;

	if (count > 1 && arr != NULL) {
		qsort(arr, (size_t)count, sizeof(char*), str_cmp_asc);
	}

	return (qd_exec_result){0};
}

// sort_desc - sort array of strings in descending order ( arr:p count:i -- )
qd_exec_result usr_str_sort_desc(qd_context* ctx) {
	qd_stack_element_t count_elem, arr_elem;

	// Pop count
	qd_stack_error err = qd_stack_pop(ctx->st, &count_elem);
	if (err != QD_STACK_OK || count_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in str::sort_desc: Expected integer count\n");
		abort();
	}

	// Pop array pointer
	err = qd_stack_pop(ctx->st, &arr_elem);
	if (err != QD_STACK_OK || arr_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in str::sort_desc: Expected pointer to string array\n");
		abort();
	}

	int64_t count = count_elem.value.i;
	char** arr = (char**)arr_elem.value.p;

	if (count > 1 && arr != NULL) {
		qsort(arr, (size_t)count, sizeof(char*), str_cmp_desc);
	}

	return (qd_exec_result){0};
}

// join - join array of strings with delimiter ( parts:p count:i delim:s -- result:s )
qd_exec_result usr_str_join(qd_context* ctx) {
	qd_stack_element_t delim_elem, count_elem, parts_elem;

	// Pop delimiter
	qd_stack_error err = qd_stack_pop(ctx->st, &delim_elem);
	if (err != QD_STACK_OK || delim_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in str::join: Expected string delimiter\n");
		abort();
	}

	// Pop count
	err = qd_stack_pop(ctx->st, &count_elem);
	if (err != QD_STACK_OK || count_elem.type != QD_STACK_TYPE_INT) {
		qd_string_release(delim_elem.value.s);
		fprintf(stderr, "Fatal error in str::join: Expected integer count\n");
		abort();
	}

	// Pop parts array
	err = qd_stack_pop(ctx->st, &parts_elem);
	if (err != QD_STACK_OK || parts_elem.type != QD_STACK_TYPE_PTR) {
		qd_string_release(delim_elem.value.s);
		fprintf(stderr, "Fatal error in str::join: Expected pointer to string array\n");
		abort();
	}

	int64_t count = count_elem.value.i;
	char** parts = (char**)parts_elem.value.p;
	const char* delim = qd_string_data(delim_elem.value.s);
	size_t delim_len = strlen(delim);

	// Handle empty array
	if (count <= 0 || parts == NULL) {
		qd_string_release(delim_elem.value.s);
		qd_push_s(ctx, "");
		qd_push_i(ctx, STR_ERR_OK);
		return (qd_exec_result){0};
	}

	// Calculate total length
	size_t total_len = 0;
	for (int64_t i = 0; i < count; i++) {
		if (parts[i] != NULL) {
			total_len += strlen(parts[i]);
		}
		if (i < count - 1) {
			total_len += delim_len;
		}
	}

	// Allocate result
	char* result = malloc(total_len + 1);
	if (!result) {
		qd_string_release(delim_elem.value.s);
		ctx->error_code = STR_ERR_ALLOC;
		if (ctx->error_msg) free(ctx->error_msg);
		ctx->error_msg = strdup("str::join: allocation failed");
		qd_push_i(ctx, STR_ERR_ALLOC);
		return (qd_exec_result){STR_ERR_ALLOC};
	}

	// Build result
	char* dest = result;
	for (int64_t i = 0; i < count; i++) {
		if (parts[i] != NULL) {
			size_t part_len = strlen(parts[i]);
			memcpy(dest, parts[i], part_len);
			dest += part_len;
		}
		if (i < count - 1) {
			memcpy(dest, delim, delim_len);
			dest += delim_len;
		}
	}
	*dest = '\0';

	qd_string_release(delim_elem.value.s);

	qd_push_s(ctx, result);
	free(result);
	qd_push_i(ctx, STR_ERR_OK);

	return (qd_exec_result){0};
}
