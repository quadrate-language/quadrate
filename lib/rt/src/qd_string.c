#define _POSIX_C_SOURCE 200809L

#include <quadrate/rt/qd_string.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

qd_string_t* qd_string_create(const char* str) {
	if (str == NULL) {
		return NULL;
	}

	size_t length = strlen(str);
	return qd_string_create_with_length(str, length);
}

qd_string_t* qd_string_create_with_length(const char* str, size_t length) {
	if (str == NULL) {
		return NULL;
	}

	qd_string_t* qd_str = (qd_string_t*)malloc(sizeof(qd_string_t));
	if (qd_str == NULL) {
		return NULL;
	}

	// Allocate with extra capacity for future growth
	// Use adaptive sizing: small capacity for tiny strings, 2x for larger
	size_t capacity;
	if (length < QD_STRING_SMALL_CAPACITY) {
		capacity = QD_STRING_SMALL_CAPACITY;
	} else {
		if (length > SIZE_MAX / 2) {
			free(qd_str);
			return NULL;
		}
		capacity = length * 2;
		if (capacity < QD_STRING_MIN_CAPACITY) {
			capacity = QD_STRING_MIN_CAPACITY;
		}
	}

	qd_str->data = (char*)malloc(capacity + 1);
	if (qd_str->data == NULL) {
		free(qd_str);
		return NULL;
	}

	memcpy(qd_str->data, str, length);
	qd_str->data[length] = '\0';
	qd_str->length = length;
	qd_str->capacity = capacity;
	qd_str->char_length = QD_STRING_CHAR_LEN_UNKNOWN;
	atomic_init(&qd_str->scan_cursor, 0);
	atomic_init(&qd_str->refcount, 1);

	return qd_str;
}

qd_string_t* qd_string_retain(qd_string_t* str) {
	if (str == NULL) {
		return NULL;
	}

	atomic_fetch_add(&str->refcount, 1);
	return str;
}

void qd_string_release(qd_string_t* str) {
	if (str == NULL) {
		return;
	}

	size_t old_count = atomic_fetch_sub(&str->refcount, 1);
	if (old_count == 1) {
		// Last reference - free the string
		free(str->data);
		free(str);
	}
}

size_t qd_string_refcount(const qd_string_t* str) {
	if (str == NULL) {
		return 0;
	}
	return atomic_load(&str->refcount);
}

const char* qd_string_data(const qd_string_t* str) {
	if (str == NULL) {
		return NULL;
	}
	return str->data;
}

size_t qd_string_length(const qd_string_t* str) {
	if (str == NULL) {
		return 0;
	}
	return str->length;
}

size_t qd_string_char_length(qd_string_t* str) {
	if (str == NULL) {
		return 0;
	}
	if (str->char_length != QD_STRING_CHAR_LEN_UNKNOWN) {
		return str->char_length;
	}

	// Count lead bytes: a continuation byte is 10xxxxxx, everything else starts a character.
	// A byte that is not part of a well-formed sequence counts as one character, which is the
	// same rule the strings module decodes by.
	size_t count = 0;
	const unsigned char* p = (const unsigned char*)str->data;
	for (size_t i = 0; i < str->length; i++) {
		if ((p[i] & 0xC0u) != 0x80u) {
			count++;
		}
	}
	str->char_length = count;
	return count;
}

size_t qd_string_char_offset(qd_string_t* str, size_t index) {
	if (str == NULL || index == 0) {
		return 0;
	}
	const size_t bytes = str->length;

	// Every character one byte: the index is the offset, no walk at all.
	if (qd_string_char_length(str) == bytes) {
		return (index < bytes) ? index : bytes;
	}

	// Resume from the last lookup when it is at or before the one being asked for, which is
	// what a forward scan always does. Both halves come from one atomic word, so they always
	// describe the same position. Offsets above 4GB do not fit the packing and simply walk.
	size_t from_char = 0;
	size_t from_byte = 0;
	if (bytes <= 0xFFFFFFFFu) {
		const uint64_t cursor = atomic_load(&str->scan_cursor);
		const size_t cur_char = (size_t)(cursor >> 32);
		const size_t cur_byte = (size_t)(cursor & 0xFFFFFFFFu);
		if (cur_char <= index && cur_byte <= bytes) {
			from_char = cur_char;
			from_byte = cur_byte;
		}
	}

	size_t off = from_byte;
	size_t seen = from_char;
	while (off < bytes && seen < index) {
		// Step over one character: the lead byte plus its continuation bytes.
		off++;
		while (off < bytes && ((const unsigned char)str->data[off] & 0xC0u) == 0x80u) {
			off++;
		}
		seen++;
	}

	if (bytes <= 0xFFFFFFFFu) {
		atomic_store(&str->scan_cursor, ((uint64_t)seen << 32) | (uint64_t)off);
	}
	return off;
}

size_t qd_string_char_index(qd_string_t* str, size_t byte_offset) {
	if (str == NULL || byte_offset == 0) {
		return 0;
	}
	const size_t bytes = str->length;
	if (byte_offset > bytes) {
		byte_offset = bytes;
	}

	// Every character one byte: the offset is the index.
	if (qd_string_char_length(str) == bytes) {
		return byte_offset;
	}

	// The inverse of qd_string_char_offset, sharing its cursor: `s needle index_of_from` in a
	// loop walks forward through the same string, so each call resumes where the last stopped
	// instead of counting from the beginning again.
	size_t from_char = 0;
	size_t from_byte = 0;
	if (bytes <= 0xFFFFFFFFu) {
		const uint64_t cursor = atomic_load(&str->scan_cursor);
		const size_t cur_char = (size_t)(cursor >> 32);
		const size_t cur_byte = (size_t)(cursor & 0xFFFFFFFFu);
		if (cur_byte <= byte_offset && cur_byte <= bytes) {
			from_char = cur_char;
			from_byte = cur_byte;
		}
	}

	size_t off = from_byte;
	size_t seen = from_char;
	while (off < byte_offset) {
		off++;
		while (off < bytes && ((const unsigned char)str->data[off] & 0xC0u) == 0x80u) {
			off++;
		}
		seen++;
	}

	if (bytes <= 0xFFFFFFFFu) {
		atomic_store(&str->scan_cursor, ((uint64_t)seen << 32) | (uint64_t)off);
	}
	return seen;
}

qd_string_t* qd_string_concat_smart(qd_string_t* str1, qd_string_t* str2) {
	if (str1 == NULL || str2 == NULL) {
		if (str1) qd_string_release(str1);
		if (str2) qd_string_release(str2);
		return NULL;
	}

	size_t len1 = str1->length;
	size_t len2 = str2->length;
	size_t total_len = len1 + len2;

	// Check if we can append in-place
	// Conditions: refcount==1 (exclusive ownership) AND enough capacity
	size_t refcount = atomic_load(&str1->refcount);
	if (refcount == 1 && str1->capacity >= total_len) {
		// In-place append - super fast!
		memcpy(str1->data + len1, str2->data, len2);
		str1->data[total_len] = '\0';
		str1->length = total_len;
		// The buffer changed, so any memo describes the old contents. The cursor stays valid
		// for the prefix that was already there, but resetting it is simpler and costs one
		// rescan at most.
		str1->char_length = QD_STRING_CHAR_LEN_UNKNOWN;
		atomic_store(&str1->scan_cursor, 0);
		qd_string_release(str2);
		return str1;
	}

	// Need to allocate new string
	// Allocate with adaptive capacity for future growth
	size_t new_capacity;
	if (total_len < QD_STRING_SMALL_CAPACITY) {
		new_capacity = QD_STRING_SMALL_CAPACITY;
	} else {
		new_capacity = total_len * 2;
		if (new_capacity < QD_STRING_MIN_CAPACITY) {
			new_capacity = QD_STRING_MIN_CAPACITY;
		}
	}

	qd_string_t* result = (qd_string_t*)malloc(sizeof(qd_string_t));
	if (result == NULL) {
		qd_string_release(str1);
		qd_string_release(str2);
		return NULL;
	}

	result->data = (char*)malloc(new_capacity + 1);
	if (result->data == NULL) {
		free(result);
		qd_string_release(str1);
		qd_string_release(str2);
		return NULL;
	}

	// Copy both strings
	memcpy(result->data, str1->data, len1);
	memcpy(result->data + len1, str2->data, len2);
	result->data[total_len] = '\0';
	result->length = total_len;
	result->capacity = new_capacity;
	result->char_length = QD_STRING_CHAR_LEN_UNKNOWN;
	atomic_init(&result->scan_cursor, 0);
	atomic_init(&result->refcount, 1);

	// Release inputs
	qd_string_release(str1);
	qd_string_release(str2);

	return result;
}

// String Builder Implementation

#define QD_SB_GROWTH_FACTOR 2

qd_string_builder_t* qd_sb_create(size_t initial_capacity) {
	if (initial_capacity == 0) {
		initial_capacity = QD_SB_DEFAULT_CAPACITY;
	}

	qd_string_builder_t* sb = (qd_string_builder_t*)malloc(sizeof(qd_string_builder_t));
	if (sb == NULL) {
		return NULL;
	}

	sb->data = (char*)malloc(initial_capacity);
	if (sb->data == NULL) {
		free(sb);
		return NULL;
	}

	sb->data[0] = '\0';
	sb->length = 0;
	sb->capacity = initial_capacity;

	return sb;
}

bool qd_sb_append(qd_string_builder_t* sb, const char* str, size_t len) {
	if (sb == NULL || str == NULL) {
		return false;
	}

	// Check if we need to grow the buffer
	size_t required = sb->length + len + 1; // +1 for null terminator
	if (required > sb->capacity) {
		// Grow capacity by factor of 2 until it fits
		size_t new_capacity = sb->capacity;
		while (new_capacity < required) {
			new_capacity *= QD_SB_GROWTH_FACTOR;
		}

		char* new_data = (char*)realloc(sb->data, new_capacity);
		if (new_data == NULL) {
			return false;
		}

		sb->data = new_data;
		sb->capacity = new_capacity;
	}

	// Append the string
	memcpy(sb->data + sb->length, str, len);
	sb->length += len;
	sb->data[sb->length] = '\0';

	return true;
}

qd_string_t* qd_sb_to_string(qd_string_builder_t* sb) {
	if (sb == NULL) {
		return NULL;
	}

	return qd_string_create_with_length(sb->data, sb->length);
}

void qd_sb_free(qd_string_builder_t* sb) {
	if (sb == NULL) {
		return;
	}

	free(sb->data);
	free(sb);
}
