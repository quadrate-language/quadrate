#define _POSIX_C_SOURCE 200809L

#include <qdrt/qd_string.h>
#include <stdlib.h>
#include <string.h>

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

	qd_str->data = (char*)malloc(length + 1);
	if (qd_str->data == NULL) {
		free(qd_str);
		return NULL;
	}

	memcpy(qd_str->data, str, length);
	qd_str->data[length] = '\0';
	qd_str->length = length;
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
