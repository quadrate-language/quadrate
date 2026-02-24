#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <qdstrings/strings.h>
#include <strings.h>
#include <qdrt/stack.h>
#include <qdrt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Error codes matching module.qd
#define STRINGS_ERR_OK 1            // Success (matches builtin Ok)
#define STRINGS_ERR_OUT_OF_BOUNDS 2 // Index out of bounds
#define STRINGS_ERR_ALLOC 3         // Allocation failed
#define STRINGS_ERR_INVALID_ARG 4   // Invalid argument

#define STRINGS_POP(ctx, elem, func_name) do { \
	if (qd_stack_pop((ctx)->st, (elem)) != QD_STACK_OK) { \
		fprintf(stderr, "Fatal error in strings::" func_name ": Stack underflow\n"); \
		abort(); \
	} \
} while(0)

// len - get string length ( str:s -- len:i )
int usr_strings_len(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::len: Stack underflow\n");
		abort();
	}

	if (val.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::len: Expected string, got type %d\n", val.type);
		abort();
	}

	size_t len = strlen(qd_string_data(val.value.s));
	qd_string_release(val.value.s);

	qd_push_i(ctx, (int64_t)len);
	return (int){0};
}

// concat - concatenate two strings ( str1:s str2:s -- result:s )
// Uses qd_string_concat_smart for in-place append when possible (refcount==1 && enough capacity)
int usr_strings_concat(qd_context* ctx) {
	qd_stack_element_t str2, str1;
	STRINGS_POP(ctx, &str2, "concat");
	STRINGS_POP(ctx, &str1, "concat");

	if (str1.type != QD_STACK_TYPE_STR || str2.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::concat: Expected two strings\n");
		if (str1.type == QD_STACK_TYPE_STR) qd_string_release(str1.value.s);
		if (str2.type == QD_STACK_TYPE_STR) qd_string_release(str2.value.s);
		abort();
	}

	// concat_smart handles in-place append when str1 has refcount==1 and enough capacity,
	// otherwise allocates a new string with 2x capacity for future growth.
	// It releases both inputs internally.
	qd_string_t* result = qd_string_concat_smart(str1.value.s, str2.value.s);

	if (!result) {
		fprintf(stderr, "Fatal error in strings::concat: Memory allocation failed\n");
		abort();
	}

	// Push result using ref to avoid an extra copy — concat_smart returns refcount=1
	qd_push_s_ref(ctx, result);
	qd_string_release(result);  // push_s_ref retained it; release our reference

	return (int){0};
}

// contains - check if string contains substring ( str:s needle:s -- contains:i )
int usr_strings_contains(qd_context* ctx) {
	qd_stack_element_t needle, haystack;
	qd_stack_error err = qd_stack_pop(ctx->st, &needle);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::contains: Stack underflow\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &haystack);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::contains: Stack underflow\n");
		if (needle.type == QD_STACK_TYPE_STR) qd_string_release(needle.value.s);
		abort();
	}

	if (haystack.type != QD_STACK_TYPE_STR || needle.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::contains: Expected two strings\n");
		if (haystack.type == QD_STACK_TYPE_STR) qd_string_release(haystack.value.s);
		if (needle.type == QD_STACK_TYPE_STR) qd_string_release(needle.value.s);
		abort();
	}

	int result = (strstr(qd_string_data(haystack.value.s), qd_string_data(needle.value.s)) != NULL) ? 1 : 0;

	qd_string_release(haystack.value.s);
	qd_string_release(needle.value.s);

	qd_push_i(ctx, result);
	return (int){0};
}

// starts_with - check if string starts with prefix ( str:s prefix:s -- result:i )
int usr_strings_starts_with(qd_context* ctx) {
	qd_stack_element_t prefix, str;
	qd_stack_error err = qd_stack_pop(ctx->st, &prefix);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::starts_with: Stack underflow\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &str);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::starts_with: Stack underflow\n");
		if (prefix.type == QD_STACK_TYPE_STR) qd_string_release(prefix.value.s);
		abort();
	}

	if (str.type != QD_STACK_TYPE_STR || prefix.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::starts_with: Expected two strings\n");
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
	return (int){0};
}

// ends_with - check if string ends with suffix ( str:s suffix:s -- result:i )
int usr_strings_ends_with(qd_context* ctx) {
	qd_stack_element_t suffix, str;
	qd_stack_error err = qd_stack_pop(ctx->st, &suffix);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::ends_with: Stack underflow\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &str);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::ends_with: Stack underflow\n");
		if (suffix.type == QD_STACK_TYPE_STR) qd_string_release(suffix.value.s);
		abort();
	}

	if (str.type != QD_STACK_TYPE_STR || suffix.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::ends_with: Expected two strings\n");
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
	return (int){0};
}

// upper - convert string to uppercase ( str:s -- result:s )
int usr_strings_upper(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::upper: Stack underflow\n");
		abort();
	}

	if (val.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::upper: Expected string, got type %d\n", val.type);
		abort();
	}

	size_t len = strlen(qd_string_data(val.value.s));
	char* result = malloc(len + 1);

	if (!result) {
		fprintf(stderr, "Fatal error in strings::upper: Memory allocation failed\n");
		qd_string_release(val.value.s);
		abort();
	}

	for (size_t i = 0; i <= len; i++) {
		result[i] = (char)toupper((unsigned char)qd_string_data(val.value.s)[i]);
	}

	qd_string_release(val.value.s);
	qd_push_s(ctx, result);
	free(result);

	return (int){0};
}

// lower - convert string to lowercase ( str:s -- result:s )
int usr_strings_lower(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::lower: Stack underflow\n");
		abort();
	}

	if (val.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::lower: Expected string, got type %d\n", val.type);
		abort();
	}

	size_t len = strlen(qd_string_data(val.value.s));
	char* result = malloc(len + 1);

	if (!result) {
		fprintf(stderr, "Fatal error in strings::lower: Memory allocation failed\n");
		qd_string_release(val.value.s);
		abort();
	}

	for (size_t i = 0; i <= len; i++) {
		result[i] = (char)tolower((unsigned char)qd_string_data(val.value.s)[i]);
	}

	qd_string_release(val.value.s);
	qd_push_s(ctx, result);
	free(result);

	return (int){0};
}

// trim - remove leading and trailing whitespace ( str:s -- result:s )
int usr_strings_trim(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::trim: Stack underflow\n");
		abort();
	}

	if (val.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::trim: Expected string, got type %d\n", val.type);
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
		fprintf(stderr, "Fatal error in strings::trim: Memory allocation failed\n");
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

	return (int){0};
}

// substring - extract substring ( str:s start:i length:i -- result:s )!
int usr_strings_substring(qd_context* ctx) {
	qd_stack_element_t len_elem, start_elem, str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &len_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::substring: Stack underflow\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &start_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::substring: Stack underflow\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::substring: Stack underflow\n");
		abort();
	}

	if (str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::substring: Expected string\n");
		abort();
	}

	if (start_elem.type != QD_STACK_TYPE_INT || len_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in strings::substring: Expected integers for start and length\n");
		qd_string_release(str_elem.value.s);
		abort();
	}

	int64_t start = start_elem.value.i;
	int64_t length = len_elem.value.i;
	size_t str_len = strlen(qd_string_data(str_elem.value.s));

	if (start < 0 || length < 0) {
		fprintf(stderr, "Fatal error in strings::substring: Negative indices not allowed\n");
		qd_string_release(str_elem.value.s);
		abort();
	}

	if ((size_t)start > str_len) {
		fprintf(stderr, "Fatal error in strings::substring: Start index out of bounds\n");
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
		fprintf(stderr, "Fatal error in strings::substring: Memory allocation failed\n");
		qd_string_release(str_elem.value.s);
		abort();
	}

	memcpy(result, qd_string_data(str_elem.value.s) + start, actual_length);
	result[actual_length] = '\0';

	qd_string_release(str_elem.value.s);
	qd_push_s(ctx, result);
	free(result);
	qd_push_i(ctx, STRINGS_ERR_OK);  // Success status for fallible function

	return (int){STRINGS_ERR_OK};
}

// split - split string by delimiter ( str:s delim:s -- parts:p count:i )
// Returns pointer to array of qd_string* and count
int usr_strings_split(qd_context* ctx) {
	qd_stack_element_t delim_elem, str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &delim_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::split: Stack underflow\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::split: Stack underflow\n");
		if (delim_elem.type == QD_STACK_TYPE_STR) qd_string_release(delim_elem.value.s);
		abort();
	}

	if (str_elem.type != QD_STACK_TYPE_STR || delim_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::split: Expected two strings\n");
		if (str_elem.type == QD_STACK_TYPE_STR) qd_string_release(str_elem.value.s);
		if (delim_elem.type == QD_STACK_TYPE_STR) qd_string_release(delim_elem.value.s);
		abort();
	}

	const char* delim = qd_string_data(delim_elem.value.s);
	size_t delim_len = strlen(delim);

	if (delim_len == 0) {
		fprintf(stderr, "Fatal error in strings::split: Empty delimiter\n");
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
		fprintf(stderr, "Fatal error in strings::split: Memory allocation failed\n");
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
			fprintf(stderr, "Fatal error in strings::split: Memory allocation failed\n");
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
		fprintf(stderr, "Fatal error in strings::split: Memory allocation failed\n");
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
	qd_push_i(ctx, STRINGS_ERR_OK);  // Success status for fallible function

	return (int){STRINGS_ERR_OK};
}

// replace - replace all occurrences ( str:s old:s new:s -- result:s )
int usr_strings_replace(qd_context* ctx) {
	qd_stack_element_t new_elem, old_elem, str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &new_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::replace: Stack underflow\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &old_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::replace: Stack underflow\n");
		if (new_elem.type == QD_STACK_TYPE_STR) qd_string_release(new_elem.value.s);
		abort();
	}
	err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::replace: Stack underflow\n");
		if (new_elem.type == QD_STACK_TYPE_STR) qd_string_release(new_elem.value.s);
		if (old_elem.type == QD_STACK_TYPE_STR) qd_string_release(old_elem.value.s);
		abort();
	}

	if (str_elem.type != QD_STACK_TYPE_STR || old_elem.type != QD_STACK_TYPE_STR ||
			new_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::replace: Expected three strings\n");
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
		qd_push_i(ctx, STRINGS_ERR_OK);  // Success status for fallible function
		return (int){STRINGS_ERR_OK};
	}

	// Count occurrences
	size_t count = 0;
	const char* pos = qd_string_data(str_elem.value.s);
	while ((pos = strstr(pos, old)) != NULL) {
		count++;
		pos += old_len;
	}

	// Calculate result length using signed arithmetic to handle shrinking replacements
	size_t str_len = strlen(qd_string_data(str_elem.value.s));
	int64_t delta = (int64_t)new_len - (int64_t)old_len;
	int64_t result_len_signed = (int64_t)str_len + (int64_t)count * delta;
	if (result_len_signed < 0) result_len_signed = 0;
	size_t result_len = (size_t)result_len_signed;

	char* result = malloc(result_len + 1);
	if (!result) {
		fprintf(stderr, "Fatal error in strings::replace: Memory allocation failed\n");
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
	qd_push_i(ctx, STRINGS_ERR_OK);  // Success status for fallible function

	return (int){STRINGS_ERR_OK};
}

// compare - compare two strings ( str1:s str2:s -- result:i )
// Returns: -1 if str1 < str2, 0 if equal, 1 if str1 > str2
int usr_strings_compare(qd_context* ctx) {
	qd_stack_element_t str2_elem, str1_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &str2_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::compare: Stack underflow\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &str1_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::compare: Stack underflow\n");
		if (str2_elem.type == QD_STACK_TYPE_STR) qd_string_release(str2_elem.value.s);
		abort();
	}

	if (str1_elem.type != QD_STACK_TYPE_STR || str2_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::compare: Expected two strings\n");
		if (str1_elem.type == QD_STACK_TYPE_STR) qd_string_release(str1_elem.value.s);
		if (str2_elem.type == QD_STACK_TYPE_STR) qd_string_release(str2_elem.value.s);
		abort();
	}

	int cmp = strcmp(qd_string_data(str1_elem.value.s), qd_string_data(str2_elem.value.s));
	int result = (cmp < 0) ? -1 : (cmp > 0) ? 1 : 0;

	qd_string_release(str1_elem.value.s);
	qd_string_release(str2_elem.value.s);

	qd_push_i(ctx, result);
	return (int){0};
}

// char_at - get character code at index ( str:s index:i -- char_code:i )!
int usr_strings_char_at(qd_context* ctx) {
	qd_stack_element_t index_elem, str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &index_elem);
	if (err != QD_STACK_OK) {
		qd_push_i(ctx, STRINGS_ERR_INVALID_ARG);
		return (int){STRINGS_ERR_INVALID_ARG};
	}
	err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		qd_push_i(ctx, STRINGS_ERR_INVALID_ARG);
		return (int){STRINGS_ERR_INVALID_ARG};
	}

	if (str_elem.type != QD_STACK_TYPE_STR) {
		qd_push_i(ctx, STRINGS_ERR_INVALID_ARG);
		return (int){STRINGS_ERR_INVALID_ARG};
	}

	if (index_elem.type != QD_STACK_TYPE_INT) {
		qd_string_release(str_elem.value.s);
		qd_push_i(ctx, STRINGS_ERR_INVALID_ARG);
		return (int){STRINGS_ERR_INVALID_ARG};
	}

	int64_t index = index_elem.value.i;
	size_t str_len = strlen(qd_string_data(str_elem.value.s));

	if (index < 0 || (size_t)index >= str_len) {
		qd_string_release(str_elem.value.s);
		ctx->error_code = STRINGS_ERR_OUT_OF_BOUNDS;
		qd_set_error_msg(ctx, "index out of bounds");
		qd_push_i(ctx, STRINGS_ERR_OUT_OF_BOUNDS);
		return (int){STRINGS_ERR_OUT_OF_BOUNDS};
	}

	int64_t char_code = (unsigned char)qd_string_data(str_elem.value.s)[index];

	qd_string_release(str_elem.value.s);

	// Push result then Ok
	qd_push_i(ctx, char_code);
	qd_push_i(ctx, STRINGS_ERR_OK);
	return (int){0};
}

// index_of - find index of substring ( haystack:s needle:s -- index:i )
int usr_strings_index_of(qd_context* ctx) {
	qd_stack_element_t needle_elem, haystack_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &needle_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::index_of: Stack underflow\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &haystack_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::index_of: Stack underflow\n");
		if (needle_elem.type == QD_STACK_TYPE_STR) qd_string_release(needle_elem.value.s);
		abort();
	}

	if (haystack_elem.type != QD_STACK_TYPE_STR || needle_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::index_of: Expected two strings\n");
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
	return (int){0};
}

// index_of_from - find index of substring starting from position ( haystack:s needle:s start:i -- index:i )
int usr_strings_index_of_from(qd_context* ctx) {
	qd_stack_element_t start_elem, needle_elem, haystack_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &start_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::index_of_from: Stack underflow\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &needle_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::index_of_from: Stack underflow\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &haystack_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::index_of_from: Stack underflow\n");
		if (needle_elem.type == QD_STACK_TYPE_STR) qd_string_release(needle_elem.value.s);
		abort();
	}

	if (haystack_elem.type != QD_STACK_TYPE_STR || needle_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::index_of_from: Expected two strings\n");
		if (haystack_elem.type == QD_STACK_TYPE_STR) qd_string_release(haystack_elem.value.s);
		if (needle_elem.type == QD_STACK_TYPE_STR) qd_string_release(needle_elem.value.s);
		abort();
	}

	if (start_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in strings::index_of_from: Expected integer start position\n");
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
	return (int){0};
}

// from_char - create string from character code ( char_code:i -- str:s )
int usr_strings_from_char(qd_context* ctx) {
	qd_stack_element_t code_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &code_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::from_char: Stack underflow\n");
		abort();
	}

	if (code_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in strings::from_char: Expected integer\n");
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

	return (int){0};
}

// from_ptr - convert C string pointer to Quadrate string ( ptr:p -- str:s )
int usr_strings_from_ptr(qd_context* ctx) {
	qd_stack_element_t ptr_elem;

	qd_stack_error err = qd_stack_pop(ctx->st, &ptr_elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::from_ptr: Stack underflow\n");
		abort();
	}

	if (ptr_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in strings::from_ptr: Expected pointer\n");
		abort();
	}

	const char* c_str = (const char*)ptr_elem.value.p;
	if (c_str == NULL) {
		qd_push_s(ctx, "");
	} else {
		qd_push_s(ctx, c_str);
	}

	return (int){0};
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
int usr_strings_sort(qd_context* ctx) {
	qd_stack_element_t count_elem, arr_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &count_elem);
	if (err != QD_STACK_OK || count_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in strings::sort: Expected integer count\n");
		abort();
	}

	// Pop array pointer
	err = qd_stack_pop(ctx->st, &arr_elem);
	if (err != QD_STACK_OK || arr_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in strings::sort: Expected pointer to string array\n");
		abort();
	}

	int64_t count = count_elem.value.i;
	char** arr = (char**)arr_elem.value.p;

	if (count > 1 && arr != NULL) {
		qsort(arr, (size_t)count, sizeof(char*), str_cmp_asc);
	}

	return (int){0};
}

// sort_desc - sort array of strings in descending order ( arr:p count:i -- )
int usr_strings_sort_desc(qd_context* ctx) {
	qd_stack_element_t count_elem, arr_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &count_elem);
	if (err != QD_STACK_OK || count_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in strings::sort_desc: Expected integer count\n");
		abort();
	}

	// Pop array pointer
	err = qd_stack_pop(ctx->st, &arr_elem);
	if (err != QD_STACK_OK || arr_elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "Fatal error in strings::sort_desc: Expected pointer to string array\n");
		abort();
	}

	int64_t count = count_elem.value.i;
	char** arr = (char**)arr_elem.value.p;

	if (count > 1 && arr != NULL) {
		qsort(arr, (size_t)count, sizeof(char*), str_cmp_desc);
	}

	return (int){0};
}

// repeat - repeat string n times ( str:s n:i -- result:s )
int usr_strings_repeat(qd_context* ctx) {
	qd_stack_element_t n_elem, str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &n_elem);
	if (err != QD_STACK_OK || n_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in strings::repeat: Expected integer count\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK || str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::repeat: Expected string\n");
		abort();
	}

	int64_t n = n_elem.value.i;
	if (n < 0) {
		n = 0;
	}

	size_t str_len = strlen(qd_string_data(str_elem.value.s));
	if (str_len > 0 && (size_t)n > SIZE_MAX / str_len) {
		fprintf(stderr, "Fatal error in strings::repeat: Result size overflow\n");
		qd_string_release(str_elem.value.s);
		abort();
	}
	size_t result_len = str_len * (size_t)n;

	char* result = malloc(result_len + 1);
	if (!result) {
		fprintf(stderr, "Fatal error in strings::repeat: Memory allocation failed\n");
		qd_string_release(str_elem.value.s);
		abort();
	}

	char* dest = result;
	for (int64_t i = 0; i < n; i++) {
		memcpy(dest, qd_string_data(str_elem.value.s), str_len);
		dest += str_len;
	}
	*dest = '\0';

	qd_string_release(str_elem.value.s);
	qd_push_s(ctx, result);
	free(result);

	return (int){0};
}

// reverse - reverse a string ( str:s -- result:s )
int usr_strings_reverse(qd_context* ctx) {
	qd_stack_element_t str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK || str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::reverse: Expected string\n");
		abort();
	}

	size_t len = strlen(qd_string_data(str_elem.value.s));
	char* result = malloc(len + 1);
	if (!result) {
		fprintf(stderr, "Fatal error in strings::reverse: Memory allocation failed\n");
		qd_string_release(str_elem.value.s);
		abort();
	}

	const char* src = qd_string_data(str_elem.value.s);
	for (size_t i = 0; i < len; i++) {
		result[i] = src[len - 1 - i];
	}
	result[len] = '\0';

	qd_string_release(str_elem.value.s);
	qd_push_s(ctx, result);
	free(result);

	return (int){0};
}

// trim_left - remove leading whitespace ( str:s -- result:s )
int usr_strings_trim_left(qd_context* ctx) {
	qd_stack_element_t str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK || str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::trim_left: Expected string\n");
		abort();
	}

	const char* src = qd_string_data(str_elem.value.s);
	while (*src && isspace((unsigned char)*src)) {
		src++;
	}

	size_t len = strlen(src);
	char* result = malloc(len + 1);
	if (!result) {
		fprintf(stderr, "Fatal error in strings::trim_left: Memory allocation failed\n");
		qd_string_release(str_elem.value.s);
		abort();
	}

	memcpy(result, src, len);
	result[len] = '\0';

	qd_string_release(str_elem.value.s);
	qd_push_s(ctx, result);
	free(result);

	return (int){0};
}

// trim_right - remove trailing whitespace ( str:s -- result:s )
int usr_strings_trim_right(qd_context* ctx) {
	qd_stack_element_t str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK || str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::trim_right: Expected string\n");
		abort();
	}

	const char* src = qd_string_data(str_elem.value.s);
	size_t len = strlen(src);

	while (len > 0 && isspace((unsigned char)src[len - 1])) {
		len--;
	}

	char* result = malloc(len + 1);
	if (!result) {
		fprintf(stderr, "Fatal error in strings::trim_right: Memory allocation failed\n");
		qd_string_release(str_elem.value.s);
		abort();
	}

	memcpy(result, src, len);
	result[len] = '\0';

	qd_string_release(str_elem.value.s);
	qd_push_s(ctx, result);
	free(result);

	return (int){0};
}

// count - count occurrences of substring ( haystack:s needle:s -- count:i )
int usr_strings_count(qd_context* ctx) {
	qd_stack_element_t needle_elem, haystack_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &needle_elem);
	if (err != QD_STACK_OK || needle_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::count: Expected string needle\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &haystack_elem);
	if (err != QD_STACK_OK || haystack_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(needle_elem.value.s);
		fprintf(stderr, "Fatal error in strings::count: Expected string haystack\n");
		abort();
	}

	const char* haystack = qd_string_data(haystack_elem.value.s);
	const char* needle = qd_string_data(needle_elem.value.s);
	size_t needle_len = strlen(needle);

	int64_t count = 0;
	if (needle_len > 0) {
		const char* pos = haystack;
		while ((pos = strstr(pos, needle)) != NULL) {
			count++;
			pos += needle_len;
		}
	}

	qd_string_release(haystack_elem.value.s);
	qd_string_release(needle_elem.value.s);

	qd_push_i(ctx, count);
	return (int){0};
}

// last_index_of - find last occurrence of substring ( haystack:s needle:s -- index:i )
int usr_strings_last_index_of(qd_context* ctx) {
	qd_stack_element_t needle_elem, haystack_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &needle_elem);
	if (err != QD_STACK_OK || needle_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::last_index_of: Expected string needle\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &haystack_elem);
	if (err != QD_STACK_OK || haystack_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(needle_elem.value.s);
		fprintf(stderr, "Fatal error in strings::last_index_of: Expected string haystack\n");
		abort();
	}

	const char* haystack = qd_string_data(haystack_elem.value.s);
	const char* needle = qd_string_data(needle_elem.value.s);
	size_t needle_len = strlen(needle);

	int64_t result = -1;
	if (needle_len > 0) {
		const char* pos = haystack;
		while ((pos = strstr(pos, needle)) != NULL) {
			result = (int64_t)(pos - haystack);
			pos += needle_len;
		}
	} else {
		// Empty needle matches at the end
		result = (int64_t)strlen(haystack);
	}

	qd_string_release(haystack_elem.value.s);
	qd_string_release(needle_elem.value.s);

	qd_push_i(ctx, result);
	return (int){0};
}

// join - join array of strings with delimiter ( parts:p count:i delim:s -- result:s )
int usr_strings_join(qd_context* ctx) {
	qd_stack_element_t delim_elem, count_elem, parts_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &delim_elem);
	if (err != QD_STACK_OK || delim_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::join: Expected string delimiter\n");
		abort();
	}
	err = qd_stack_pop(ctx->st, &count_elem);
	if (err != QD_STACK_OK || count_elem.type != QD_STACK_TYPE_INT) {
		qd_string_release(delim_elem.value.s);
		fprintf(stderr, "Fatal error in strings::join: Expected integer count\n");
		abort();
	}

	// Pop parts array
	err = qd_stack_pop(ctx->st, &parts_elem);
	if (err != QD_STACK_OK || parts_elem.type != QD_STACK_TYPE_PTR) {
		qd_string_release(delim_elem.value.s);
		fprintf(stderr, "Fatal error in strings::join: Expected pointer to string array\n");
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
		qd_push_i(ctx, STRINGS_ERR_OK);
		return (int){0};
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
		ctx->error_code = STRINGS_ERR_ALLOC;
		qd_set_error_msg(ctx, "strings::join: allocation failed");
		qd_push_i(ctx, STRINGS_ERR_ALLOC);
		return (int){STRINGS_ERR_ALLOC};
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
	qd_push_i(ctx, STRINGS_ERR_OK);

	return (int){0};
}

// is_empty - check if string is empty ( str:s -- result:i )
int usr_strings_is_empty(qd_context* ctx) {
	qd_stack_element_t str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK || str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::is_empty: Expected string\n");
		abort();
	}

	int result = (strlen(qd_string_data(str_elem.value.s)) == 0) ? 1 : 0;
	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result);
	return (int){0};
}

// is_blank - check if string is empty or only whitespace ( str:s -- result:i )
int usr_strings_is_blank(qd_context* ctx) {
	qd_stack_element_t str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK || str_elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::is_blank: Expected string\n");
		abort();
	}

	const char* s = qd_string_data(str_elem.value.s);
	int result = 1;
	while (*s) {
		if (!isspace((unsigned char)*s)) {
			result = 0;
			break;
		}
		s++;
	}
	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result);
	return (int){0};
}

// equals_ignore_case - case-insensitive comparison ( a:s b:s -- result:i )
int usr_strings_equals_ignore_case(qd_context* ctx) {
	qd_stack_element_t b_elem, a_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &b_elem);
	if (err != QD_STACK_OK) abort();
	err = qd_stack_pop(ctx->st, &a_elem);
	if (err != QD_STACK_OK) {
		if (b_elem.type == QD_STACK_TYPE_STR) qd_string_release(b_elem.value.s);
		abort();
	}

	if (a_elem.type != QD_STACK_TYPE_STR || b_elem.type != QD_STACK_TYPE_STR) {
		if (a_elem.type == QD_STACK_TYPE_STR) qd_string_release(a_elem.value.s);
		if (b_elem.type == QD_STACK_TYPE_STR) qd_string_release(b_elem.value.s);
		abort();
	}

	int result = (strcasecmp(qd_string_data(a_elem.value.s), qd_string_data(b_elem.value.s)) == 0) ? 1 : 0;
	qd_string_release(a_elem.value.s);
	qd_string_release(b_elem.value.s);
	qd_push_i(ctx, result);
	return (int){0};
}

// pad_left - left-pad string to length ( str:s len:i ch:s -- result:s )
int usr_strings_pad_left(qd_context* ctx) {
	qd_stack_element_t ch_elem, len_elem, str_elem;
	STRINGS_POP(ctx, &ch_elem, "pad_left");
	STRINGS_POP(ctx, &len_elem, "pad_left");
	STRINGS_POP(ctx, &str_elem, "pad_left");

	const char* str = qd_string_data(str_elem.value.s);
	int64_t target_len = len_elem.value.i;
	const char* pad_ch = qd_string_data(ch_elem.value.s);
	size_t str_len = strlen(str);

	if ((int64_t)str_len >= target_len || strlen(pad_ch) == 0) {
		qd_push_s(ctx, str);
		qd_string_release(str_elem.value.s);
		qd_string_release(ch_elem.value.s);
		return (int){0};
	}

	size_t pad_count = (size_t)target_len - str_len;
	char* result = malloc((size_t)target_len + 1);
	if (!result) abort();

	for (size_t i = 0; i < pad_count; i++) {
		result[i] = pad_ch[0];
	}
	memcpy(result + pad_count, str, str_len + 1);

	qd_string_release(str_elem.value.s);
	qd_string_release(ch_elem.value.s);
	qd_push_s(ctx, result);
	free(result);
	return (int){0};
}

// pad_right - right-pad string to length ( str:s len:i ch:s -- result:s )
int usr_strings_pad_right(qd_context* ctx) {
	qd_stack_element_t ch_elem, len_elem, str_elem;
	STRINGS_POP(ctx, &ch_elem, "pad_right");
	STRINGS_POP(ctx, &len_elem, "pad_right");
	STRINGS_POP(ctx, &str_elem, "pad_right");

	const char* str = qd_string_data(str_elem.value.s);
	int64_t target_len = len_elem.value.i;
	const char* pad_ch = qd_string_data(ch_elem.value.s);
	size_t str_len = strlen(str);

	if ((int64_t)str_len >= target_len || strlen(pad_ch) == 0) {
		qd_push_s(ctx, str);
		qd_string_release(str_elem.value.s);
		qd_string_release(ch_elem.value.s);
		return (int){0};
	}

	size_t pad_count = (size_t)target_len - str_len;
	char* result = malloc((size_t)target_len + 1);
	if (!result) abort();

	memcpy(result, str, str_len);
	for (size_t i = 0; i < pad_count; i++) {
		result[str_len + i] = pad_ch[0];
	}
	result[target_len] = '\0';

	qd_string_release(str_elem.value.s);
	qd_string_release(ch_elem.value.s);
	qd_push_s(ctx, result);
	free(result);
	return (int){0};
}

// center - center string with padding ( str:s len:i ch:s -- result:s )
int usr_strings_center(qd_context* ctx) {
	qd_stack_element_t ch_elem, len_elem, str_elem;
	STRINGS_POP(ctx, &ch_elem, "center");
	STRINGS_POP(ctx, &len_elem, "center");
	STRINGS_POP(ctx, &str_elem, "center");

	const char* str = qd_string_data(str_elem.value.s);
	int64_t target_len = len_elem.value.i;
	const char* pad_ch = qd_string_data(ch_elem.value.s);
	size_t str_len = strlen(str);

	if ((int64_t)str_len >= target_len || strlen(pad_ch) == 0) {
		qd_push_s(ctx, str);
		qd_string_release(str_elem.value.s);
		qd_string_release(ch_elem.value.s);
		return (int){0};
	}

	size_t total_pad = (size_t)target_len - str_len;
	size_t left_pad = total_pad / 2;
	size_t right_pad = total_pad - left_pad;

	char* result = malloc((size_t)target_len + 1);
	if (!result) abort();

	for (size_t i = 0; i < left_pad; i++) result[i] = pad_ch[0];
	memcpy(result + left_pad, str, str_len);
	for (size_t i = 0; i < right_pad; i++) result[left_pad + str_len + i] = pad_ch[0];
	result[target_len] = '\0';

	qd_string_release(str_elem.value.s);
	qd_string_release(ch_elem.value.s);
	qd_push_s(ctx, result);
	free(result);
	return (int){0};
}

// capitalize - uppercase first char ( str:s -- result:s )
int usr_strings_capitalize(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "capitalize");

	const char* str = qd_string_data(str_elem.value.s);
	size_t len = strlen(str);

	char* result = malloc(len + 1);
	if (!result) abort();

	if (len > 0) {
		result[0] = (char)toupper((unsigned char)str[0]);
		for (size_t i = 1; i < len; i++) {
			result[i] = (char)tolower((unsigned char)str[i]);
		}
	}
	result[len] = '\0';

	qd_string_release(str_elem.value.s);
	qd_push_s(ctx, result);
	free(result);
	return (int){0};
}

// title - title case ( str:s -- result:s )
int usr_strings_title(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "title");

	const char* str = qd_string_data(str_elem.value.s);
	size_t len = strlen(str);

	char* result = malloc(len + 1);
	if (!result) abort();

	int capitalize_next = 1;
	for (size_t i = 0; i < len; i++) {
		if (isspace((unsigned char)str[i])) {
			result[i] = str[i];
			capitalize_next = 1;
		} else if (capitalize_next) {
			result[i] = (char)toupper((unsigned char)str[i]);
			capitalize_next = 0;
		} else {
			result[i] = (char)tolower((unsigned char)str[i]);
		}
	}
	result[len] = '\0';

	qd_string_release(str_elem.value.s);
	qd_push_s(ctx, result);
	free(result);
	return (int){0};
}

// trim_prefix - remove prefix if present ( str:s prefix:s -- result:s )
int usr_strings_trim_prefix(qd_context* ctx) {
	qd_stack_element_t prefix_elem, str_elem;
	STRINGS_POP(ctx, &prefix_elem, "trim_prefix");
	STRINGS_POP(ctx, &str_elem, "trim_prefix");

	const char* str = qd_string_data(str_elem.value.s);
	const char* prefix = qd_string_data(prefix_elem.value.s);
	size_t str_len = strlen(str);
	size_t prefix_len = strlen(prefix);

	if (prefix_len <= str_len && strncmp(str, prefix, prefix_len) == 0) {
		qd_push_s(ctx, str + prefix_len);
	} else {
		qd_push_s(ctx, str);
	}

	qd_string_release(str_elem.value.s);
	qd_string_release(prefix_elem.value.s);
	return (int){0};
}

// trim_suffix - remove suffix if present ( str:s suffix:s -- result:s )
int usr_strings_trim_suffix(qd_context* ctx) {
	qd_stack_element_t suffix_elem, str_elem;
	STRINGS_POP(ctx, &suffix_elem, "trim_suffix");
	STRINGS_POP(ctx, &str_elem, "trim_suffix");

	const char* str = qd_string_data(str_elem.value.s);
	const char* suffix = qd_string_data(suffix_elem.value.s);
	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);

	if (suffix_len <= str_len && strcmp(str + str_len - suffix_len, suffix) == 0) {
		char* result = malloc(str_len - suffix_len + 1);
		if (!result) abort();
		memcpy(result, str, str_len - suffix_len);
		result[str_len - suffix_len] = '\0';
		qd_push_s(ctx, result);
		free(result);
	} else {
		qd_push_s(ctx, str);
	}

	qd_string_release(str_elem.value.s);
	qd_string_release(suffix_elem.value.s);
	return (int){0};
}

// replace_first - replace first occurrence only ( str:s old:s new:s -- result:s )
int usr_strings_replace_first(qd_context* ctx) {
	qd_stack_element_t new_elem, old_elem, str_elem;
	STRINGS_POP(ctx, &new_elem, "replace_first");
	STRINGS_POP(ctx, &old_elem, "replace_first");
	STRINGS_POP(ctx, &str_elem, "replace_first");

	const char* str = qd_string_data(str_elem.value.s);
	const char* old = qd_string_data(old_elem.value.s);
	const char* new = qd_string_data(new_elem.value.s);
	size_t old_len = strlen(old);
	size_t new_len = strlen(new);

	const char* pos = strstr(str, old);
	if (pos == NULL || old_len == 0) {
		qd_push_s(ctx, str);
	} else {
		size_t str_len = strlen(str);
		size_t result_len = str_len - old_len + new_len;
		char* result = malloc(result_len + 1);
		if (!result) abort();

		size_t prefix_len = (size_t)(pos - str);
		memcpy(result, str, prefix_len);
		memcpy(result + prefix_len, new, new_len);
		strcpy(result + prefix_len + new_len, pos + old_len);

		qd_push_s(ctx, result);
		free(result);
	}

	qd_string_release(str_elem.value.s);
	qd_string_release(old_elem.value.s);
	qd_string_release(new_elem.value.s);
	return (int){0};
}

// insert - insert string at position ( str:s pos:i ins:s -- result:s )
int usr_strings_insert(qd_context* ctx) {
	qd_stack_element_t ins_elem, pos_elem, str_elem;
	STRINGS_POP(ctx, &ins_elem, "insert");
	STRINGS_POP(ctx, &pos_elem, "insert");
	STRINGS_POP(ctx, &str_elem, "insert");

	const char* str = qd_string_data(str_elem.value.s);
	int64_t pos = pos_elem.value.i;
	const char* ins = qd_string_data(ins_elem.value.s);
	size_t str_len = strlen(str);
	size_t ins_len = strlen(ins);

	if (pos < 0) pos = 0;
	if ((size_t)pos > str_len) pos = (int64_t)str_len;

	char* result = malloc(str_len + ins_len + 1);
	if (!result) abort();

	memcpy(result, str, (size_t)pos);
	memcpy(result + pos, ins, ins_len);
	strcpy(result + pos + ins_len, str + pos);

	qd_string_release(str_elem.value.s);
	qd_string_release(ins_elem.value.s);
	qd_push_s(ctx, result);
	free(result);
	return (int){0};
}

// remove_range - remove range from string ( str:s start:i len:i -- result:s )
int usr_strings_remove_range(qd_context* ctx) {
	qd_stack_element_t len_elem, start_elem, str_elem;
	STRINGS_POP(ctx, &len_elem, "remove_range");
	STRINGS_POP(ctx, &start_elem, "remove_range");
	STRINGS_POP(ctx, &str_elem, "remove_range");

	const char* str = qd_string_data(str_elem.value.s);
	int64_t start = start_elem.value.i;
	int64_t remove_len = len_elem.value.i;
	size_t str_len = strlen(str);

	if (start < 0) start = 0;
	if ((size_t)start >= str_len || remove_len <= 0) {
		qd_push_s(ctx, str);
		qd_string_release(str_elem.value.s);
		return (int){0};
	}
	if ((size_t)(start + remove_len) > str_len) {
		remove_len = (int64_t)str_len - start;
	}

	size_t result_len = str_len - (size_t)remove_len;
	char* result = malloc(result_len + 1);
	if (!result) abort();

	memcpy(result, str, (size_t)start);
	strcpy(result + start, str + start + remove_len);

	qd_string_release(str_elem.value.s);
	qd_push_s(ctx, result);
	free(result);
	return (int){0};
}

// truncate - truncate to max length with optional suffix ( str:s max:i suffix:s -- result:s )
int usr_strings_truncate(qd_context* ctx) {
	qd_stack_element_t suffix_elem, max_elem, str_elem;
	STRINGS_POP(ctx, &suffix_elem, "truncate");
	STRINGS_POP(ctx, &max_elem, "truncate");
	STRINGS_POP(ctx, &str_elem, "truncate");

	const char* str = qd_string_data(str_elem.value.s);
	int64_t max_len = max_elem.value.i;
	const char* suffix = qd_string_data(suffix_elem.value.s);
	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);

	if ((int64_t)str_len <= max_len || max_len < 0) {
		qd_push_s(ctx, str);
	} else {
		size_t trunc_len = (size_t)max_len;
		if (suffix_len > 0 && trunc_len > suffix_len) {
			trunc_len -= suffix_len;
		}
		char* result = malloc((size_t)max_len + 1);
		if (!result) abort();

		memcpy(result, str, trunc_len);
		if (suffix_len > 0) {
			memcpy(result + trunc_len, suffix, suffix_len);
		}
		result[trunc_len + suffix_len] = '\0';

		qd_push_s(ctx, result);
		free(result);
	}

	qd_string_release(str_elem.value.s);
	qd_string_release(suffix_elem.value.s);
	return (int){0};
}

// lines - split by newlines ( str:s -- arr:p count:i )!
int usr_strings_lines(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "lines");

	const char* str = qd_string_data(str_elem.value.s);

	// Count lines
	size_t count = 1;
	for (const char* p = str; *p; p++) {
		if (*p == '\n') count++;
	}

	qd_string_t** parts = malloc(count * sizeof(qd_string_t*));
	if (!parts) abort();

	size_t idx = 0;
	const char* start = str;
	for (const char* p = str; ; p++) {
		if (*p == '\n' || *p == '\0') {
			size_t len = (size_t)(p - start);
			// Strip \r if present
			if (len > 0 && start[len - 1] == '\r') len--;
			parts[idx++] = qd_string_create_with_length(start, len);
			start = p + 1;
			if (*p == '\0') break;
		}
	}

	qd_string_release(str_elem.value.s);
	qd_push_p(ctx, parts);
	qd_push_i(ctx, (int64_t)idx);
	qd_push_i(ctx, STRINGS_ERR_OK);
	return (int){0};
}

// words - split by whitespace ( str:s -- arr:p count:i )!
int usr_strings_words(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "words");

	const char* str = qd_string_data(str_elem.value.s);

	// Count words
	size_t count = 0;
	int in_word = 0;
	for (const char* p = str; *p; p++) {
		if (isspace((unsigned char)*p)) {
			in_word = 0;
		} else if (!in_word) {
			in_word = 1;
			count++;
		}
	}

	if (count == 0) {
		qd_string_release(str_elem.value.s);
		qd_push_p(ctx, NULL);
		qd_push_i(ctx, 0);
		qd_push_i(ctx, STRINGS_ERR_OK);
		return (int){0};
	}

	qd_string_t** parts = malloc(count * sizeof(qd_string_t*));
	if (!parts) abort();

	size_t idx = 0;
	const char* start = NULL;
	for (const char* p = str; ; p++) {
		if (*p == '\0' || isspace((unsigned char)*p)) {
			if (start != NULL) {
				size_t len = (size_t)(p - start);
				parts[idx++] = qd_string_create_with_length(start, len);
				start = NULL;
			}
			if (*p == '\0') break;
		} else if (start == NULL) {
			start = p;
		}
	}

	qd_string_release(str_elem.value.s);
	qd_push_p(ctx, parts);
	qd_push_i(ctx, (int64_t)idx);
	qd_push_i(ctx, STRINGS_ERR_OK);
	return (int){0};
}

// split_n - split into at most n parts ( str:s delim:s n:i -- parts:p count:i )!
int usr_strings_split_n(qd_context* ctx) {
	qd_stack_element_t n_elem, delim_elem, str_elem;
	STRINGS_POP(ctx, &n_elem, "split_n");
	STRINGS_POP(ctx, &delim_elem, "split_n");
	STRINGS_POP(ctx, &str_elem, "split_n");

	const char* str = qd_string_data(str_elem.value.s);
	const char* delim = qd_string_data(delim_elem.value.s);
	int64_t max_parts = n_elem.value.i;
	size_t delim_len = strlen(delim);

	if (max_parts <= 0 || delim_len == 0) {
		qd_string_t** parts = malloc(sizeof(qd_string_t*));
		if (!parts) abort();
		parts[0] = qd_string_create(str);
		qd_string_release(str_elem.value.s);
		qd_string_release(delim_elem.value.s);
		qd_push_p(ctx, parts);
		qd_push_i(ctx, 1);
		qd_push_i(ctx, STRINGS_ERR_OK);
		return (int){0};
	}

	// Count parts (up to max)
	size_t count = 1;
	const char* pos = str;
	while (count < (size_t)max_parts && (pos = strstr(pos, delim)) != NULL) {
		count++;
		pos += delim_len;
	}

	qd_string_t** parts = malloc(count * sizeof(qd_string_t*));
	if (!parts) abort();

	size_t idx = 0;
	const char* start = str;
	pos = str;

	while (idx < count - 1 && (pos = strstr(pos, delim)) != NULL) {
		size_t part_len = (size_t)(pos - start);
		parts[idx++] = qd_string_create_with_length(start, part_len);
		pos += delim_len;
		start = pos;
	}
	// Last part (remainder)
	parts[idx] = qd_string_create(start);

	qd_string_release(str_elem.value.s);
	qd_string_release(delim_elem.value.s);
	qd_push_p(ctx, parts);
	qd_push_i(ctx, (int64_t)count);
	qd_push_i(ctx, STRINGS_ERR_OK);
	return (int){0};
}

// is_numeric - check if all digits ( str:s -- result:i )
int usr_strings_is_numeric(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "is_numeric");

	const char* str = qd_string_data(str_elem.value.s);
	int result = (*str != '\0') ? 1 : 0;
	for (const char* p = str; *p && result; p++) {
		if (!isdigit((unsigned char)*p)) result = 0;
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result);
	return (int){0};
}

// is_alpha - check if all alphabetic ( str:s -- result:i )
int usr_strings_is_alpha(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "is_alpha");

	const char* str = qd_string_data(str_elem.value.s);
	int result = (*str != '\0') ? 1 : 0;
	for (const char* p = str; *p && result; p++) {
		if (!isalpha((unsigned char)*p)) result = 0;
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result);
	return (int){0};
}

// is_alphanumeric - check if all alphanumeric ( str:s -- result:i )
int usr_strings_is_alphanumeric(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "is_alphanumeric");

	const char* str = qd_string_data(str_elem.value.s);
	int result = (*str != '\0') ? 1 : 0;
	for (const char* p = str; *p && result; p++) {
		if (!isalnum((unsigned char)*p)) result = 0;
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result);
	return (int){0};
}

// is_ascii - check if all ASCII (0-127) ( str:s -- result:i )
int usr_strings_is_ascii(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "is_ascii");

	const char* str = qd_string_data(str_elem.value.s);
	int result = 1;
	for (const char* p = str; *p && result; p++) {
		if ((unsigned char)*p > 127) result = 0;
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result);
	return (int){0};
}

// is_lowercase - check if all lowercase ( str:s -- result:i )
int usr_strings_is_lowercase(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "is_lowercase");

	const char* str = qd_string_data(str_elem.value.s);
	int has_letters = 0;
	int result = 1;
	for (const char* p = str; *p && result; p++) {
		if (isupper((unsigned char)*p)) result = 0;
		if (isalpha((unsigned char)*p)) has_letters = 1;
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result && has_letters);
	return (int){0};
}

// is_uppercase - check if all uppercase ( str:s -- result:i )
int usr_strings_is_uppercase(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "is_uppercase");

	const char* str = qd_string_data(str_elem.value.s);
	int has_letters = 0;
	int result = 1;
	for (const char* p = str; *p && result; p++) {
		if (islower((unsigned char)*p)) result = 0;
		if (isalpha((unsigned char)*p)) has_letters = 1;
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result && has_letters);
	return (int){0};
}

// char_count - count UTF-8 codepoints ( str:s -- count:i )
int usr_strings_char_count(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "char_count");

	const char* str = qd_string_data(str_elem.value.s);
	int64_t count = 0;

	for (const unsigned char* p = (const unsigned char*)str; *p; p++) {
		// Count only leading bytes (not continuation bytes 10xxxxxx)
		if ((*p & 0xC0) != 0x80) count++;
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, count);
	return (int){0};
}

// slice - substring with negative index support ( str:s start:i end:i -- result:s )
int usr_strings_slice(qd_context* ctx) {
	qd_stack_element_t end_elem, start_elem, str_elem;
	STRINGS_POP(ctx, &end_elem, "slice");
	STRINGS_POP(ctx, &start_elem, "slice");
	STRINGS_POP(ctx, &str_elem, "slice");

	const char* str = qd_string_data(str_elem.value.s);
	int64_t start = start_elem.value.i;
	int64_t end = end_elem.value.i;
	int64_t str_len = (int64_t)strlen(str);

	// Handle negative indices
	if (start < 0) start = str_len + start;
	if (end < 0) end = str_len + end;

	// Clamp to valid range
	if (start < 0) start = 0;
	if (end > str_len) end = str_len;
	if (start > str_len) start = str_len;

	if (end <= start) {
		qd_push_s(ctx, "");
	} else {
		size_t result_len = (size_t)(end - start);
		char* result = malloc(result_len + 1);
		if (!result) abort();
		memcpy(result, str + start, result_len);
		result[result_len] = '\0';
		qd_push_s(ctx, result);
		free(result);
	}

	qd_string_release(str_elem.value.s);
	return (int){0};
}

// column - format strings into columns ( arr:p count:i widths:p num_cols:i -- result:s )
int usr_strings_column(qd_context* ctx) {
	qd_stack_element_t num_cols_elem, widths_elem, count_elem, arr_elem;
	STRINGS_POP(ctx, &num_cols_elem, "column");
	STRINGS_POP(ctx, &widths_elem, "column");
	STRINGS_POP(ctx, &count_elem, "column");
	STRINGS_POP(ctx, &arr_elem, "column");

	char** arr = (char**)arr_elem.value.p;
	int64_t count = count_elem.value.i;
	int64_t* widths = (int64_t*)widths_elem.value.p;
	int64_t num_cols = num_cols_elem.value.i;

	if (count <= 0 || num_cols <= 0 || arr == NULL || widths == NULL) {
		qd_push_s(ctx, "");
		return (int){0};
	}

	// Calculate total width per row (sum of widths + spaces between)
	size_t row_width = 0;
	for (int64_t c = 0; c < num_cols; c++) {
		row_width += (size_t)widths[c];
		if (c < num_cols - 1) row_width += 1;  // space between columns
	}
	row_width += 1;  // newline

	int64_t num_rows = (count + num_cols - 1) / num_cols;
	size_t result_size = row_width * (size_t)num_rows + 1;

	char* result = malloc(result_size);
	if (!result) abort();

	char* dest = result;
	int64_t idx = 0;

	for (int64_t row = 0; row < num_rows; row++) {
		for (int64_t col = 0; col < num_cols; col++) {
			int64_t width = widths[col];
			const char* cell = (idx < count && arr[idx]) ? arr[idx] : "";
			size_t cell_len = strlen(cell);

			// Copy cell content (truncate if needed)
			size_t copy_len = (cell_len < (size_t)width) ? cell_len : (size_t)width;
			memcpy(dest, cell, copy_len);
			dest += copy_len;

			// Pad with spaces
			for (size_t p = copy_len; p < (size_t)width; p++) {
				*dest++ = ' ';
			}

			// Space between columns (except last)
			if (col < num_cols - 1) *dest++ = ' ';
			idx++;
		}
		*dest++ = '\n';
	}
	// Remove trailing newline
	if (dest > result) dest--;
	*dest = '\0';

	qd_push_s(ctx, result);
	free(result);
	return (int){0};
}
