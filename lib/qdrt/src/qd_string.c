#define _POSIX_C_SOURCE 200809L

#include <qdrt/qd_string.h>
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

	// Allocate with extra capacity for future growth (2x or minimum 16 bytes)
	size_t capacity = length * 2;
	if (capacity < 16) {
		capacity = 16;
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
		qd_string_release(str2);
		return str1;
	}

	// Need to allocate new string
	// Allocate with 2x capacity for future growth
	size_t new_capacity = total_len * 2;
	if (new_capacity < 16) {
		new_capacity = 16;
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
	atomic_init(&result->refcount, 1);

	// Release inputs
	qd_string_release(str1);
	qd_string_release(str2);

	return result;
}

// String Builder Implementation

#define QD_SB_DEFAULT_CAPACITY 64
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
