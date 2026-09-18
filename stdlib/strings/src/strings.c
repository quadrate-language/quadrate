#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <quadrate/strings/strings.h>
#include <quadrate/unicode/codepoint.h>
#include <quadrate/rt/array.h>
#include <quadrate/rt/stack.h>
#include <quadrate/rt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define STRINGS_POP(ctx, elem, func_name) do { \
	if (qd_stack_pop((ctx)->st, (elem)) != QD_STACK_OK) { \
		fprintf(stderr, "Fatal error in strings::" func_name ": Stack underflow\n"); \
		abort(); \
	} \
} while(0)

/* `str` is a UTF-8 string (specification 3.1), so every index and length in this module counts
 * codepoints, not bytes, and no operation may cut a multi-byte sequence in half.
 *
 * Nothing here converts between character indices and byte offsets by hand. That goes through
 * qd_string_char_length, qd_string_char_offset and qd_string_char_index, which memoise on the
 * string: a conversion is a walk, and doing it per call made any loop that scanned or sliced a
 * large string quadratic. Going through them uniformly is the point -- one function still
 * counting for itself is the loop that gets written next.
 *
 * The UTF-8 encoding primitives and the codepoint case/classification tables live in
 * <quadrate/unicode/codepoint.h>, shared with the unicode module so the two layers cannot
 * disagree about a character. */


/* Map every codepoint through `map` into a fresh NUL-terminated buffer; NULL on allocation
 * failure, and *out_bytes gets the result length. The result can differ in byte length from
 * the input (U+017F long s uppercases to a one-byte 'S'), so the buffer is sized for the
 * worst case rather than assumed equal.
 *
 * A byte that is not part of a well-formed sequence is copied through untouched: case
 * mapping a broken byte is meaningless, and inventing a codepoint for it would rewrite the
 * caller's data. */
static char* utf8_map_case(const char* src, size_t bytes, uint32_t (*map)(uint32_t), size_t* out_bytes) {
	char* out = malloc(bytes * 4 + 1);
	if (out == NULL) {
		return NULL;
	}
	size_t w = 0;
	size_t i = 0;
	while (i < bytes) {
		uint32_t cp = 0;
		const size_t n = utf8_decode((const unsigned char*)src + i, bytes - i, &cp);
		if (n == 1 && (unsigned char)src[i] >= 0x80u) {
			out[w++] = src[i];
		} else {
			const size_t m = utf8_encode(map(cp), out + w);
			if (m == 0) {
				memcpy(out + w, src + i, n);
				w += n;
			} else {
				w += m;
			}
		}
		i += n;
	}
	out[w] = '\0';
	if (out_bytes != NULL) {
		*out_bytes = w;
	}
	return out;
}

/* Byte offset just past the leading whitespace of `s`. Unicode spaces count as well as the
 * ASCII ones -- a non-breaking space (U+00A0) is whitespace a user pasted in, and the
 * byte-wise isspace() left it in place. */
static size_t utf8_skip_space_forward(const char* s, size_t bytes) {
	size_t i = 0;
	while (i < bytes) {
		uint32_t cp = 0;
		const size_t n = utf8_decode((const unsigned char*)s + i, bytes - i, &cp);
		if (!utf8_is_space_cp(cp)) {
			break;
		}
		i += n;
	}
	return i;
}

/* Byte offset of the end of `s` once trailing whitespace is removed, never going below
 * `floor_off`. Scanning backwards means finding each character's start first, which is what
 * the continuation-byte test does. */
static size_t utf8_skip_space_backward(const char* s, size_t bytes, size_t floor_off) {
	size_t end = bytes;
	while (end > floor_off) {
		size_t start = end - 1;
		while (start > floor_off && ((unsigned char)s[start] & 0xC0u) == 0x80u) {
			start--;
		}
		uint32_t cp = 0;
		utf8_decode((const unsigned char*)s + start, end - start, &cp);
		if (!utf8_is_space_cp(cp)) {
			break;
		}
		end = start;
	}
	return end;
}

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

	// Codepoints, not bytes: `str` is UTF-8 (specification 3.1), so "héllo" is 5 characters
	// even though it occupies 6 bytes. strings::byte_len gives the encoded size.
	// The count is memoised on the string, so repeated calls in a loop cost nothing.
	const int64_t len = (int64_t)qd_string_char_length(val.value.s);
	qd_string_release(val.value.s);

	qd_push_i(ctx, len);
	return (int){0};
}

// data - get raw char* pointer to string data ( s:str -- p:ptr )
int usr_strings_data(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in strings::data: Stack underflow\n");
		abort();
	}

	if (val.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in strings::data: Expected string, got type %d\n", val.type);
		abort();
	}

	const char* data = qd_string_data(val.value.s);
	// Push the raw pointer — caller's local variable still holds a reference
	// so the string data remains valid
	qd_push_p(ctx, (void*)data);
	qd_string_release(val.value.s);
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

	size_t str_len = qd_string_length(str.value.s);
	size_t prefix_len = qd_string_length(prefix.value.s);

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

	size_t str_len = qd_string_length(str.value.s);
	size_t suffix_len = qd_string_length(suffix.value.s);

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

	// Per codepoint, not per byte. toupper() on a byte left every non-ASCII letter alone, so
	// "héllo wörld" uppercased to "HéLLO WöRLD" -- the two letters that most needed it
	// were the two it skipped.
	size_t len = qd_string_length(val.value.s);
	char* result = utf8_map_case(qd_string_data(val.value.s), len, utf8_to_upper_cp, NULL);

	if (!result) {
		fprintf(stderr, "Fatal error in strings::upper: Memory allocation failed\n");
		qd_string_release(val.value.s);
		abort();
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

	size_t len = qd_string_length(val.value.s);
	char* result = utf8_map_case(qd_string_data(val.value.s), len, utf8_to_lower_cp, NULL);

	if (!result) {
		fprintf(stderr, "Fatal error in strings::lower: Memory allocation failed\n");
		qd_string_release(val.value.s);
		abort();
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

	const char* base = qd_string_data(val.value.s);
	const size_t base_bytes = strlen(base);

	// Whitespace is decided per codepoint, so Unicode spaces are trimmed too.
	const size_t begin_off = utf8_skip_space_forward(base, base_bytes);
	const size_t end_off = utf8_skip_space_backward(base, base_bytes, begin_off);

	const char* start = base + begin_off;
	size_t trimmed_len = end_off - begin_off;

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
	size_t str_len = qd_string_length(str_elem.value.s);

	// An out-of-range cut is the error this function declares, so it is returned rather than
	// aborted on. It used to abort, which made the `switch` every caller wraps it in dead code:
	// the process was gone before any arm ran. Use strings::slice for the clamping form that
	// cannot fail at all.
	if (start < 0 || length < 0) {
		qd_string_release(str_elem.value.s);
		ctx->error_code = STRINGS_ERR_OUT_OF_BOUNDS;
		qd_set_error_msg(ctx, "index out of bounds");
		qd_push_i(ctx, STRINGS_ERR_OUT_OF_BOUNDS);
		return (int){STRINGS_ERR_OUT_OF_BOUNDS};
	}

	if ((size_t)start > str_len) {
		qd_string_release(str_elem.value.s);
		ctx->error_code = STRINGS_ERR_OUT_OF_BOUNDS;
		qd_set_error_msg(ctx, "index out of bounds");
		qd_push_i(ctx, STRINGS_ERR_OUT_OF_BOUNDS);
		return (int){STRINGS_ERR_OUT_OF_BOUNDS};
	}

	// `start` and `length` count codepoints, so the cut always lands on a character boundary.
	// Slicing by byte let a cut fall inside a sequence: "héllo" 0 2 substring produced the two
	// bytes 'h' 0xC3, a truncated é that is not valid UTF-8.
	const char* src_data = qd_string_data(str_elem.value.s);
	// Character indices, so a cut always lands on a character boundary. The conversion is
	// memoised on the string (see qd_string_char_offset), which is what keeps a .qd loop
	// slicing forward through a large string linear rather than quadratic.
	(void)src_data;
	const size_t begin = qd_string_char_offset(str_elem.value.s, (size_t)start);
	const size_t end = qd_string_char_offset(str_elem.value.s, (size_t)start + (size_t)length);
	const size_t actual_length = end - begin;

	char* result = malloc(actual_length + 1);
	if (!result) {
		fprintf(stderr, "Fatal error in strings::substring: Memory allocation failed\n");
		qd_string_release(str_elem.value.s);
		abort();
	}

	memcpy(result, src_data + begin, actual_length);
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

	// A real Quadrate array, not a bare malloc'd qd_string_t**. The old shape could
	// not be indexed from Quadrate at all -- `parts i nth` segfaulted -- and
	// strings::join read the same buffer as char**, so split|join produced garbage.
	qd_array_t* parts_arr = qd_array_create(count, QD_ARRAY_TYPE_STR);
	if (!parts_arr) {
		fprintf(stderr, "Fatal error in strings::split: Memory allocation failed\n");
		qd_string_release(str_elem.value.s);
		qd_string_release(delim_elem.value.s);
		abort();
	}
	qd_string_t** parts = (qd_string_t**)parts_arr->data.p;

	// Split string
	size_t idx = 0;
	const char* start = qd_string_data(str_elem.value.s);
	pos = qd_string_data(str_elem.value.s);

	while ((pos = strstr(pos, delim)) != NULL) {
		size_t part_len = (size_t)(pos - start);
		parts[idx] = qd_string_create_with_length(start, part_len);
		if (!parts[idx]) {
			fprintf(stderr, "Fatal error in strings::split: Memory allocation failed\n");
			parts_arr->length = idx;
			qd_array_release(parts_arr);
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
		parts_arr->length = idx;
		qd_array_release(parts_arr);
		qd_string_release(str_elem.value.s);
		qd_string_release(delim_elem.value.s);
		abort();
	}

	qd_string_release(str_elem.value.s);
	qd_string_release(delim_elem.value.s);

	parts_arr->length = count;
	qd_push_p(ctx, parts_arr);
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
	size_t str_len = qd_string_length(str_elem.value.s);
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

// char_at - get character code at index ( str:s index:i -- char_code:i )
//
// Total: an index outside the string gives NotAChar (-1) rather than an error. Scanners are the
// whole readership of this function, and they all read "the character here, if there is one" --
// json's, uri's and path's loops each bounds-check before every call. As a fallible function it
// had to be called with `!` from those non-fallible scanners, so a single missing check aborted
// the host process: json::get_int on the malformed document {"a":1, killed it outright. -1 is
// not a codepoint, so it compares equal to no character a scanner tests for and the loop ends on
// its own; the same sentinel convention as strings::index_of, and the same totality as
// strings::slice, which is substring's non-aborting counterpart.
int usr_strings_char_at(qd_context* ctx) {
	qd_stack_element_t index_elem, str_elem;
	STRINGS_POP(ctx, &index_elem, "char_at");
	STRINGS_POP(ctx, &str_elem, "char_at");

	if (str_elem.type != QD_STACK_TYPE_STR || index_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in strings::char_at: Expected a string and an integer\n");
		if (str_elem.type == QD_STACK_TYPE_STR) qd_string_release(str_elem.value.s);
		abort();
	}

	int64_t index = index_elem.value.i;
	const char* data = qd_string_data(str_elem.value.s);
	const size_t bytes = qd_string_length(str_elem.value.s);

	// `index` counts codepoints, and the result is a codepoint. Indexing bytes returned 195
	// for character 1 of "héllo" -- the first half of the é -- which is not a character
	// the string contains.
	//
	// Locating a codepoint is a walk from the start, which would make a .qd loop that reads a
	// string one character at a time quadratic -- json's scan over an MCP request body took
	// minutes. When the memoised codepoint count equals the byte length every character
	// occupies one byte, so the index is already the offset and no walk is needed; that covers
	// ASCII, which is nearly all of the strings a scan like that runs over.
	if (index < 0) {
		qd_string_release(str_elem.value.s);
		qd_push_i(ctx, STRINGS_NOT_A_CHAR);
		return (int){0};
	}
	const size_t offset = qd_string_char_offset(str_elem.value.s, (size_t)index);
	if (offset >= bytes) {
		qd_string_release(str_elem.value.s);
		qd_push_i(ctx, STRINGS_NOT_A_CHAR);
		return (int){0};
	}

	uint32_t cp = 0;
	utf8_decode((const unsigned char*)data + offset, bytes - offset, &cp);
	int64_t char_code = (int64_t)cp;

	qd_string_release(str_elem.value.s);

	qd_push_i(ctx, char_code);
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

	// The search itself is safe on bytes -- UTF-8 is self-synchronising, so a match can only
	// begin on a character boundary -- but the *index* returned must count codepoints, to
	// match strings::len and char_at. It used to be a byte offset, so "héllo wörld" reported
	// 8 for "ö" where the character index is 7.
	int64_t result = -1;
	if (pos != NULL) {
		result = (int64_t)qd_string_char_index(haystack_elem.value.s, (size_t)(pos - haystack));
	}

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

	// `start` and the result are both codepoint indices, so the byte offset to search from is
	// computed rather than used directly -- starting at byte `start` could land mid-character.
	int64_t result = -1;
	if (start >= 0) {
		const size_t start_byte = qd_string_char_offset(haystack_elem.value.s, (size_t)start);
		if (start_byte < haystack_len) {
			const char* pos = strstr(haystack + start_byte, needle);
			if (pos != NULL) {
				result = (int64_t)qd_string_char_index(haystack_elem.value.s, (size_t)(pos - haystack));
			}
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

	// Encode the codepoint as UTF-8. This used to truncate to one byte ("just handle
	// single-byte values"), so from_char of anything above 127 produced a lone continuation
	// or lead byte -- not a character, and not even valid UTF-8. A codepoint outside the
	// Unicode range, or a surrogate, yields the empty string rather than invalid output.
	char result[5];
	const size_t n = utf8_encode((char_code >= 0 && char_code <= 0x10FFFF) ? (uint32_t)char_code : 0xFFFFFFFFu,
			result);
	result[n] = '\0';
	qd_push_s(ctx, result);

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
/* Elements are qd_string_t*, the shape every string-list producer in the stdlib
 * now hands back (strings::split/split_n/lines/words, os::list/glob). */
static int str_cmp_asc(const void* a, const void* b) {
	qd_string_t* str_a = *(qd_string_t* const*)a;
	qd_string_t* str_b = *(qd_string_t* const*)b;
	return strcmp(qd_string_data(str_a), qd_string_data(str_b));
}

// Comparison function for qsort (descending)
static int str_cmp_desc(const void* a, const void* b) {
	qd_string_t* str_a = *(qd_string_t* const*)a;
	qd_string_t* str_b = *(qd_string_t* const*)b;
	return strcmp(qd_string_data(str_b), qd_string_data(str_a));
}

/* Fetch the element buffer of a Quadrate string array taken off the stack.
 * Returns NULL and reports if the pointer is not an array -- the string-list
 * functions require one now that every producer emits one. */
static qd_string_t** strings_array_elems(const qd_stack_element_t* elem, const char* who) {
	if (elem->value.p == NULL) {
		return NULL;
	}
	if (!qd_array_is_valid(elem->value.p)) {
		fprintf(stderr, "Fatal error in strings::%s: expected a string array (from split/lines/os::list)\n", who);
		abort();
	}
	return (qd_string_t**)((qd_array_t*)elem->value.p)->data.p;
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
	qd_string_t** arr = strings_array_elems(&arr_elem, "sort");

	if (count > 1 && arr != NULL) {
		qsort(arr, (size_t)count, sizeof(qd_string_t*), str_cmp_asc);
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
	qd_string_t** arr = strings_array_elems(&arr_elem, "sort_desc");

	if (count > 1 && arr != NULL) {
		qsort(arr, (size_t)count, sizeof(qd_string_t*), str_cmp_desc);
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

	size_t str_len = qd_string_length(str_elem.value.s);
	if (str_len > 0 && (size_t)n > SIZE_MAX / str_len) {
		fprintf(stderr, "Fatal error in strings::repeat: Result size overflow\n");
		qd_string_release(str_elem.value.s);
		abort();
	}
	size_t result_len = str_len * (size_t)n;
	if (result_len == SIZE_MAX) {
		// result_len + 1 would wrap to 0, yielding an undersized allocation.
		fprintf(stderr, "Fatal error in strings::repeat: Result size overflow\n");
		qd_string_release(str_elem.value.s);
		abort();
	}

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

	size_t len = qd_string_length(str_elem.value.s);
	char* result = malloc(len + 1);
	if (!result) {
		fprintf(stderr, "Fatal error in strings::reverse: Memory allocation failed\n");
		qd_string_release(str_elem.value.s);
		abort();
	}

	// Reverse whole codepoints. Reversing bytes turned "héllo wörld" into mojibake: every
	// multi-byte character came out with its bytes backwards, which is not a character at all,
	// so the result was not valid UTF-8.
	const char* src = qd_string_data(str_elem.value.s);
	size_t out = len;
	size_t i = 0;
	while (i < len) {
		uint32_t cp = 0;
		const size_t n = utf8_decode((const unsigned char*)src + i, len - i, &cp);
		out -= n;
		memcpy(result + out, src + i, n);
		i += n;
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
	src += utf8_skip_space_forward(src, strlen(src));

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
	size_t len = utf8_skip_space_backward(src, strlen(src), 0);

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
		const char* found = NULL;
		while ((pos = strstr(pos, needle)) != NULL) {
			found = pos;
			pos += needle_len;
		}
		if (found != NULL) {
			result = (int64_t)qd_string_char_index(haystack_elem.value.s, (size_t)(found - haystack));
		}
	} else {
		// Empty needle matches at the end, which in codepoints is the character count
		result = (int64_t)qd_string_char_length(haystack_elem.value.s);
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
	const char* delim = qd_string_data(delim_elem.value.s);
	size_t delim_len = strlen(delim);

	// count <= 0 short-circuits before the pointer is examined at all: joining
	// nothing is "" whatever was passed, and callers rely on that (see
	// tests/qd/strings/join_edge.qd, which passes a scratch buffer with count 0).
	if (count <= 0) {
		qd_string_release(delim_elem.value.s);
		qd_push_s(ctx, "");
		qd_push_i(ctx, STRINGS_ERR_OK);
		return (int){0};
	}

	// Elements are qd_string_t*, the one shape every string-list producer emits.
	// This briefly accepted a raw char** as well, because os::list returned one;
	// os::list now returns an array too, so there is a single convention.
	qd_string_t** parts = strings_array_elems(&parts_elem, "join");
	if (parts == NULL) {
		qd_string_release(delim_elem.value.s);
		qd_push_s(ctx, "");
		qd_push_i(ctx, STRINGS_ERR_OK);
		return (int){0};
	}

	// Calculate total length
	size_t total_len = 0;
	for (int64_t i = 0; i < count; i++) {
		const char* part = parts[i] != NULL ? qd_string_data(parts[i]) : NULL;
		if (part != NULL) {
			total_len += strlen(part);
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
		const char* part = parts[i] != NULL ? qd_string_data(parts[i]) : NULL;
		if (part != NULL) {
			size_t part_len = strlen(part);
			memcpy(dest, part, part_len);
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

	int result = (qd_string_length(str_elem.value.s) == 0) ? 1 : 0;
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
		uint32_t cp = 0;
		utf8_decode((const unsigned char*)s, strlen(s), &cp);
		if (!utf8_is_space_cp(cp)) {
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

	// strcasecmp folds ASCII only, so "STRASSE" and "strasse" matched but "ÄPPLE" and "äpple"
	// did not. Fold both sides per codepoint and compare the results.
	const char* a_str = qd_string_data(a_elem.value.s);
	const char* b_str = qd_string_data(b_elem.value.s);
	size_t a_folded_len = 0;
	size_t b_folded_len = 0;
	char* a_folded = utf8_map_case(a_str, strlen(a_str), utf8_to_lower_cp, &a_folded_len);
	char* b_folded = utf8_map_case(b_str, strlen(b_str), utf8_to_lower_cp, &b_folded_len);
	int result = 0;
	if (a_folded != NULL && b_folded != NULL) {
		result = (a_folded_len == b_folded_len && memcmp(a_folded, b_folded, a_folded_len) == 0) ? 1 : 0;
	}
	free(a_folded);
	free(b_folded);
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

	// Width is measured in characters, and the pad is one whole character. Both used to be
	// bytes: an accented string was padded too little, and a non-ASCII pad character was
	// emitted as `pad_ch[0]` -- its first byte on its own, which is not a character.
	const int64_t str_chars = (int64_t)qd_string_char_length(str_elem.value.s);
	const size_t pad_bytes = (strlen(pad_ch) > 0) ? utf8_seq_len((unsigned char)pad_ch[0]) : 0;

	if (str_chars >= target_len || pad_bytes == 0) {
		qd_push_s(ctx, str);
		qd_string_release(str_elem.value.s);
		qd_string_release(ch_elem.value.s);
		return (int){0};
	}

	size_t pad_count = (size_t)(target_len - str_chars);
	char* result = malloc(pad_count * pad_bytes + str_len + 1);
	if (!result) abort();

	for (size_t i = 0; i < pad_count; i++) {
		memcpy(result + i * pad_bytes, pad_ch, pad_bytes);
	}
	memcpy(result + pad_count * pad_bytes, str, str_len + 1);

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

	// Characters, not bytes -- see pad_left.
	const int64_t str_chars = (int64_t)qd_string_char_length(str_elem.value.s);
	const size_t pad_bytes = (strlen(pad_ch) > 0) ? utf8_seq_len((unsigned char)pad_ch[0]) : 0;

	if (str_chars >= target_len || pad_bytes == 0) {
		qd_push_s(ctx, str);
		qd_string_release(str_elem.value.s);
		qd_string_release(ch_elem.value.s);
		return (int){0};
	}

	size_t pad_count = (size_t)(target_len - str_chars);
	char* result = malloc(str_len + pad_count * pad_bytes + 1);
	if (!result) abort();

	memcpy(result, str, str_len);
	for (size_t i = 0; i < pad_count; i++) {
		memcpy(result + str_len + i * pad_bytes, pad_ch, pad_bytes);
	}
	result[str_len + pad_count * pad_bytes] = '\0';

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

	// Characters, not bytes -- see pad_left.
	const int64_t str_chars = (int64_t)qd_string_char_length(str_elem.value.s);
	const size_t pad_bytes = (strlen(pad_ch) > 0) ? utf8_seq_len((unsigned char)pad_ch[0]) : 0;

	if (str_chars >= target_len || pad_bytes == 0) {
		qd_push_s(ctx, str);
		qd_string_release(str_elem.value.s);
		qd_string_release(ch_elem.value.s);
		return (int){0};
	}

	size_t total_pad = (size_t)(target_len - str_chars);
	size_t left_pad = total_pad / 2;
	size_t right_pad = total_pad - left_pad;

	char* result = malloc(total_pad * pad_bytes + str_len + 1);
	if (!result) abort();

	size_t w = 0;
	for (size_t i = 0; i < left_pad; i++) {
		memcpy(result + w, pad_ch, pad_bytes);
		w += pad_bytes;
	}
	memcpy(result + w, str, str_len);
	w += str_len;
	for (size_t i = 0; i < right_pad; i++) {
		memcpy(result + w, pad_ch, pad_bytes);
		w += pad_bytes;
	}
	result[w] = '\0';

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

	// Per codepoint: the first character uppercases, the rest lowercase. Byte-wise this both
	// missed every non-ASCII letter and, for a string starting with one, would have written a
	// mapped byte over the lead byte of a sequence.
	char* result = malloc(len * 4 + 1);
	if (!result) abort();

	size_t w = 0;
	size_t i = 0;
	int first = 1;
	while (i < len) {
		uint32_t cp = 0;
		const size_t n = utf8_decode((const unsigned char*)str + i, len - i, &cp);
		if (n == 1 && (unsigned char)str[i] >= 0x80u) {
			result[w++] = str[i];
		} else {
			const uint32_t mapped = first ? utf8_to_upper_cp(cp) : utf8_to_lower_cp(cp);
			const size_t m = utf8_encode(mapped, result + w);
			if (m == 0) {
				memcpy(result + w, str + i, n);
				w += n;
			} else {
				w += m;
			}
		}
		first = 0;
		i += n;
	}
	result[w] = '\0';

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

	// Per codepoint, with word boundaries decided by the same whitespace test the trim family
	// uses.
	char* result = malloc(len * 4 + 1);
	if (!result) abort();

	size_t w = 0;
	size_t i = 0;
	int capitalize_next = 1;
	while (i < len) {
		uint32_t cp = 0;
		const size_t n = utf8_decode((const unsigned char*)str + i, len - i, &cp);
		if (n == 1 && (unsigned char)str[i] >= 0x80u) {
			result[w++] = str[i];
		} else if (utf8_is_space_cp(cp)) {
			memcpy(result + w, str + i, n);
			w += n;
			capitalize_next = 1;
		} else {
			const uint32_t mapped = capitalize_next ? utf8_to_upper_cp(cp) : utf8_to_lower_cp(cp);
			const size_t m = utf8_encode(mapped, result + w);
			if (m == 0) {
				memcpy(result + w, str + i, n);
				w += n;
			} else {
				w += m;
			}
			capitalize_next = 0;
		}
		i += n;
	}
	result[w] = '\0';

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

	// `pos` is a codepoint index, so the insertion point is converted to a byte offset -- a
	// raw byte position could split a character and leave two invalid fragments around the
	// inserted text.
	if (pos < 0) pos = 0;
	const size_t pos_byte = qd_string_char_offset(str_elem.value.s, (size_t)pos);

	char* result = malloc(str_len + ins_len + 1);
	if (!result) abort();

	memcpy(result, str, pos_byte);
	memcpy(result + pos_byte, ins, ins_len);
	strcpy(result + pos_byte + ins_len, str + pos_byte);

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

	// Both indices count codepoints; the range is turned into byte offsets so the cut lands
	// on character boundaries at each end.
	if (start < 0) start = 0;
	const size_t start_byte = qd_string_char_offset(str_elem.value.s, (size_t)start);
	if (start_byte >= str_len || remove_len <= 0) {
		qd_push_s(ctx, str);
		qd_string_release(str_elem.value.s);
		return (int){0};
	}
	const size_t end_byte = qd_string_char_offset(str_elem.value.s, (size_t)(start + remove_len));

	size_t result_len = str_len - (end_byte - start_byte);
	char* result = malloc(result_len + 1);
	if (!result) abort();

	memcpy(result, str, start_byte);
	strcpy(result + start_byte, str + end_byte);

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
	size_t suffix_len = strlen(suffix);

	// `max_len` is a character budget, and the suffix costs characters out of it. Measuring
	// in bytes truncated a string of accented text far earlier than asked and could cut
	// inside a character.
	const int64_t str_chars = (int64_t)qd_string_char_length(str_elem.value.s);
	const int64_t suffix_chars = (int64_t)qd_string_char_length(suffix_elem.value.s);

	if (str_chars <= max_len || max_len < 0) {
		qd_push_s(ctx, str);
	} else {
		int64_t keep_chars = max_len;
		if (suffix_chars > 0 && keep_chars > suffix_chars) {
			keep_chars -= suffix_chars;
		}
		const size_t keep_bytes = qd_string_char_offset(str_elem.value.s, (size_t)keep_chars);
		char* result = malloc(keep_bytes + suffix_len + 1);
		if (!result) abort();

		memcpy(result, str, keep_bytes);
		if (suffix_len > 0) {
			memcpy(result + keep_bytes, suffix, suffix_len);
		}
		result[keep_bytes + suffix_len] = '\0';

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

	// A real Quadrate array, matching strings::split. As a bare malloc'd buffer this
	// validated as an array of length 0, so `len` answered 0 while `count` answered
	// the true number -- a loop over the result silently did nothing.
	qd_array_t* parts_arr = qd_array_create(count, QD_ARRAY_TYPE_STR);
	if (!parts_arr) abort();
	qd_string_t** parts = (qd_string_t**)parts_arr->data.p;

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

	parts_arr->length = idx;
	qd_string_release(str_elem.value.s);
	qd_push_p(ctx, parts_arr);
	qd_push_i(ctx, (int64_t)idx);
	qd_push_i(ctx, STRINGS_ERR_OK);
	return (int){0};
}

// words - split by whitespace ( str:s -- arr:p count:i )!
int usr_strings_words(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "words");

	const char* str = qd_string_data(str_elem.value.s);

	// Word boundaries use the same codepoint whitespace test as trim and title, so a string
	// separated by non-breaking spaces splits the way it looks like it should. Splitting on
	// ASCII whitespace bytes alone was already safe for UTF-8 -- a space byte cannot occur
	// inside a multi-byte sequence -- but it was inconsistent with the rest of the module.
	const size_t bytes = strlen(str);
	size_t count = 0;
	int in_word = 0;
	for (size_t i = 0; i < bytes;) {
		uint32_t cp = 0;
		const size_t n = utf8_decode((const unsigned char*)str + i, bytes - i, &cp);
		if (utf8_is_space_cp(cp)) {
			in_word = 0;
		} else if (!in_word) {
			in_word = 1;
			count++;
		}
		i += n;
	}

	if (count == 0) {
		qd_string_release(str_elem.value.s);
		qd_push_p(ctx, NULL);
		qd_push_i(ctx, 0);
		qd_push_i(ctx, STRINGS_ERR_OK);
		return (int){0};
	}

	qd_array_t* parts_arr = qd_array_create(count, QD_ARRAY_TYPE_STR);
	if (!parts_arr) abort();
	qd_string_t** parts = (qd_string_t**)parts_arr->data.p;

	size_t idx = 0;
	int have_start = 0;
	size_t start_off = 0;
	for (size_t i = 0; i <= bytes;) {
		const int at_end = (i == bytes);
		uint32_t cp = 0;
		size_t n = 1;
		if (!at_end) {
			n = utf8_decode((const unsigned char*)str + i, bytes - i, &cp);
		}
		if (at_end || utf8_is_space_cp(cp)) {
			if (have_start) {
				parts[idx++] = qd_string_create_with_length(str + start_off, i - start_off);
				have_start = 0;
			}
			if (at_end) {
				break;
			}
		} else if (!have_start) {
			have_start = 1;
			start_off = i;
		}
		i += n;
	}

	parts_arr->length = idx;
	qd_string_release(str_elem.value.s);
	qd_push_p(ctx, parts_arr);
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
		qd_array_t* one = qd_array_create(1, QD_ARRAY_TYPE_STR);
		if (!one) abort();
		one->data.p[0] = qd_string_create(str);
		one->length = 1;
		qd_string_release(str_elem.value.s);
		qd_string_release(delim_elem.value.s);
		qd_push_p(ctx, one);
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

	// Same shape as strings::split: a real Quadrate array, so `nth`/`len` work and
	// strings::join reads the elements correctly.
	qd_array_t* parts_arr = qd_array_create(count, QD_ARRAY_TYPE_STR);
	if (!parts_arr) abort();
	qd_string_t** parts = (qd_string_t**)parts_arr->data.p;

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

	parts_arr->length = count;
	qd_string_release(str_elem.value.s);
	qd_string_release(delim_elem.value.s);
	qd_push_p(ctx, parts_arr);
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
	for (size_t i = 0, n = strlen(str); i < n && result;) {
		uint32_t cp = 0;
		i += utf8_decode((const unsigned char*)str + i, n - i, &cp);
		if (!utf8_is_digit_cp(cp)) result = 0;
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
	// Per codepoint. isalpha() on bytes answered "no" for every non-ASCII letter, so
	// "h\xc3\xa9llo" was not alphabetic and "\xe6\x97\xa5\xe6\x9c\xac" was not either.
	int result = (*str != '\0') ? 1 : 0;
	for (size_t i = 0, n = strlen(str); i < n && result;) {
		uint32_t cp = 0;
		i += utf8_decode((const unsigned char*)str + i, n - i, &cp);
		if (!utf8_is_alpha_cp(cp)) result = 0;
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
	for (size_t i = 0, n = strlen(str); i < n && result;) {
		uint32_t cp = 0;
		i += utf8_decode((const unsigned char*)str + i, n - i, &cp);
		if (!utf8_is_alpha_cp(cp) && !utf8_is_digit_cp(cp)) result = 0;
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
	for (size_t i = 0, n = strlen(str); i < n && result;) {
		uint32_t cp = 0;
		i += utf8_decode((const unsigned char*)str + i, n - i, &cp);
		if (utf8_is_upper_cp(cp)) result = 0;
		if (utf8_is_alpha_cp(cp)) has_letters = 1;
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
	for (size_t i = 0, n = strlen(str); i < n && result;) {
		uint32_t cp = 0;
		i += utf8_decode((const unsigned char*)str + i, n - i, &cp);
		if (utf8_is_lower_cp(cp)) result = 0;
		if (utf8_is_alpha_cp(cp)) has_letters = 1;
	}

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, result && has_letters);
	return (int){0};
}

// byte_len - size of the UTF-8 encoding ( str:s -- bytes:i )
//
// This is what strings::len used to return. len now counts characters, which is the useful
// answer nearly everywhere; the byte size still matters when sizing a buffer or writing to
// something that counts octets, so it keeps a name of its own rather than being reachable
// only through strings::data. It replaces char_count, which counted codepoints and so became
// an exact synonym for len.
int usr_strings_byte_len(qd_context* ctx) {
	qd_stack_element_t str_elem;
	STRINGS_POP(ctx, &str_elem, "byte_len");

	const int64_t bytes = (int64_t)qd_string_length(str_elem.value.s);

	qd_string_release(str_elem.value.s);
	qd_push_i(ctx, bytes);
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
	// Indices count codepoints here too, including the negative ones -- -1 means the last
	// character, not the last byte.
	int64_t str_len = (int64_t)qd_string_char_length(str_elem.value.s);

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
		const size_t begin_byte = qd_string_char_offset(str_elem.value.s, (size_t)start);
		const size_t end_byte = qd_string_char_offset(str_elem.value.s, (size_t)end);
		size_t result_len = end_byte - begin_byte;
		char* result = malloc(result_len + 1);
		if (!result) abort();
		memcpy(result, str + begin_byte, result_len);
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

	qd_string_t** arr = strings_array_elems(&arr_elem, "column");
	int64_t count = count_elem.value.i;
	int64_t* widths = (int64_t*)widths_elem.value.p;
	int64_t num_cols = num_cols_elem.value.i;

	if (count <= 0 || num_cols <= 0 || arr == NULL || widths == NULL) {
		qd_push_s(ctx, "");
		return (int){0};
	}

	// Calculate total width per row (sum of widths + spaces between).
	// Widths are caller-supplied: reject negatives (which would wrap when cast
	// to size_t) and guard every accumulation/multiplication against overflow,
	// otherwise an undersized buffer would be written past below.
	size_t row_width = 0;
	for (int64_t c = 0; c < num_cols; c++) {
		if (widths[c] < 0) {
			qd_push_s(ctx, "");
			return (int){0};
		}
		size_t w = (size_t)widths[c];
		size_t sep = (c < num_cols - 1) ? 1 : 0;  // space between columns
		if (row_width > SIZE_MAX - w - sep) {
			qd_push_s(ctx, "");
			return (int){0};
		}
		row_width += w + sep;
	}
	if (row_width > SIZE_MAX - 1) {
		qd_push_s(ctx, "");
		return (int){0};
	}
	row_width += 1;  // newline

	int64_t num_rows = (count + num_cols - 1) / num_cols;
	if (num_rows <= 0 || row_width > (SIZE_MAX - 1) / (size_t)num_rows) {
		qd_push_s(ctx, "");
		return (int){0};
	}
	size_t result_size = row_width * (size_t)num_rows + 1;

	char* result = malloc(result_size);
	if (!result) abort();

	char* dest = result;
	int64_t idx = 0;

	for (int64_t row = 0; row < num_rows; row++) {
		for (int64_t col = 0; col < num_cols; col++) {
			int64_t width = widths[col];
			const char* cell = (idx < count && arr[idx] != NULL) ? qd_string_data(arr[idx]) : "";
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
