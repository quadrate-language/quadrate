/**
 * @file array.c
 * @brief Dynamic array implementation for Quadrate runtime
 */

#include <qdrt/array.h>
#include <qdrt/qd_string.h>
#include <qdrt/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

qd_array_t* qd_array_create(size_t capacity, qd_array_type elemType) {
	if (capacity == 0) {
		capacity = 8; // Default initial capacity
	}

	qd_array_t* arr = (qd_array_t*)malloc(sizeof(qd_array_t));
	if (!arr) {
		return NULL;
	}

	arr->refcount = 1;
	arr->length = 0;
	arr->capacity = capacity;
	arr->elemType = elemType;

	// Allocate data based on element type
	size_t elemSize;
	switch (elemType) {
	case QD_ARRAY_TYPE_INT:
		elemSize = sizeof(int64_t);
		arr->data.i = (int64_t*)malloc(capacity * elemSize);
		if (!arr->data.i) {
			free(arr);
			return NULL;
		}
		break;
	case QD_ARRAY_TYPE_FLOAT:
		elemSize = sizeof(double);
		arr->data.f = (double*)malloc(capacity * elemSize);
		if (!arr->data.f) {
			free(arr);
			return NULL;
		}
		break;
	case QD_ARRAY_TYPE_STR:
	case QD_ARRAY_TYPE_PTR:
		elemSize = sizeof(void*);
		arr->data.p = (void**)malloc(capacity * elemSize);
		if (!arr->data.p) {
			free(arr);
			return NULL;
		}
		memset(arr->data.p, 0, capacity * elemSize);
		break;
	default:
		free(arr);
		return NULL;
	}

	return arr;
}

void qd_array_retain(qd_array_t* arr) {
	if (arr) {
		arr->refcount++;
	}
}

void qd_array_release(qd_array_t* arr) {
	if (!arr) {
		return;
	}

	arr->refcount--;
	if (arr->refcount > 0) {
		return;
	}

	// Free contents
	switch (arr->elemType) {
	case QD_ARRAY_TYPE_INT:
		free(arr->data.i);
		break;
	case QD_ARRAY_TYPE_FLOAT:
		free(arr->data.f);
		break;
	case QD_ARRAY_TYPE_STR:
		// Release all string references
		for (size_t i = 0; i < arr->length; i++) {
			if (arr->data.p[i]) {
				qd_string_release((qd_string_t*)arr->data.p[i]);
			}
		}
		free(arr->data.p);
		break;
	case QD_ARRAY_TYPE_PTR:
		free(arr->data.p);
		break;
	}

	free(arr);
}

size_t qd_array_length(const qd_array_t* arr) {
	return arr ? arr->length : 0;
}

int qd_array_get_int(const qd_array_t* arr, size_t index, int64_t* value) {
	if (!arr || !value || index >= arr->length || arr->elemType != QD_ARRAY_TYPE_INT) {
		return -1;
	}
	*value = arr->data.i[index];
	return 0;
}

int qd_array_get_float(const qd_array_t* arr, size_t index, double* value) {
	if (!arr || !value || index >= arr->length || arr->elemType != QD_ARRAY_TYPE_FLOAT) {
		return -1;
	}
	*value = arr->data.f[index];
	return 0;
}

int qd_array_get_ptr(const qd_array_t* arr, size_t index, void** value) {
	if (!arr || !value || index >= arr->length ||
		(arr->elemType != QD_ARRAY_TYPE_PTR && arr->elemType != QD_ARRAY_TYPE_STR)) {
		return -1;
	}
	*value = arr->data.p[index];
	return 0;
}

int qd_array_set_int(qd_array_t* arr, size_t index, int64_t value) {
	if (!arr || index >= arr->length || arr->elemType != QD_ARRAY_TYPE_INT) {
		return -1;
	}
	arr->data.i[index] = value;
	return 0;
}

int qd_array_set_float(qd_array_t* arr, size_t index, double value) {
	if (!arr || index >= arr->length || arr->elemType != QD_ARRAY_TYPE_FLOAT) {
		return -1;
	}
	arr->data.f[index] = value;
	return 0;
}

int qd_array_set_ptr(qd_array_t* arr, size_t index, void* value) {
	if (!arr || index >= arr->length ||
		(arr->elemType != QD_ARRAY_TYPE_PTR && arr->elemType != QD_ARRAY_TYPE_STR)) {
		return -1;
	}

	// For string arrays, handle reference counting
	if (arr->elemType == QD_ARRAY_TYPE_STR) {
		// Release old string if present
		if (arr->data.p[index]) {
			qd_string_release((qd_string_t*)arr->data.p[index]);
		}
		// Retain new string
		if (value) {
			qd_string_retain((qd_string_t*)value);
		}
	}

	arr->data.p[index] = value;
	return 0;
}

// Helper to grow array capacity
static int qd_array_grow(qd_array_t* arr) {
	size_t newCapacity = arr->capacity * 2;
	if (newCapacity < 8) {
		newCapacity = 8;
	}

	switch (arr->elemType) {
	case QD_ARRAY_TYPE_INT: {
		int64_t* newData = (int64_t*)realloc(arr->data.i, newCapacity * sizeof(int64_t));
		if (!newData) {
			return -1;
		}
		arr->data.i = newData;
		break;
	}
	case QD_ARRAY_TYPE_FLOAT: {
		double* newData = (double*)realloc(arr->data.f, newCapacity * sizeof(double));
		if (!newData) {
			return -1;
		}
		arr->data.f = newData;
		break;
	}
	case QD_ARRAY_TYPE_STR:
	case QD_ARRAY_TYPE_PTR: {
		void** newData = (void**)realloc(arr->data.p, newCapacity * sizeof(void*));
		if (!newData) {
			return -1;
		}
		// Zero-initialize new slots
		memset(newData + arr->capacity, 0, (newCapacity - arr->capacity) * sizeof(void*));
		arr->data.p = newData;
		break;
	}
	default:
		return -1;
	}

	arr->capacity = newCapacity;
	return 0;
}

int qd_array_push_int(qd_array_t* arr, int64_t value) {
	if (!arr || arr->elemType != QD_ARRAY_TYPE_INT) {
		return -1;
	}

	if (arr->length >= arr->capacity) {
		if (qd_array_grow(arr) != 0) {
			return -1;
		}
	}

	arr->data.i[arr->length++] = value;
	return 0;
}

int qd_array_push_float(qd_array_t* arr, double value) {
	if (!arr || arr->elemType != QD_ARRAY_TYPE_FLOAT) {
		return -1;
	}

	if (arr->length >= arr->capacity) {
		if (qd_array_grow(arr) != 0) {
			return -1;
		}
	}

	arr->data.f[arr->length++] = value;
	return 0;
}

int qd_array_push_ptr(qd_array_t* arr, void* value) {
	if (!arr || (arr->elemType != QD_ARRAY_TYPE_PTR && arr->elemType != QD_ARRAY_TYPE_STR)) {
		return -1;
	}

	if (arr->length >= arr->capacity) {
		if (qd_array_grow(arr) != 0) {
			return -1;
		}
	}

	// For string arrays, retain the string
	if (arr->elemType == QD_ARRAY_TYPE_STR && value) {
		qd_string_retain((qd_string_t*)value);
	}

	arr->data.p[arr->length++] = value;
	return 0;
}

// Stack-based array operations

qd_exec_result qd_len(qd_context* ctx) {
	qd_exec_result result = {0};
	if (!ctx || !ctx->st) {
		result.code = -1;
		return result;
	}

	qd_stack_element_t elem;
	if (qd_stack_pop(ctx->st, &elem) != QD_STACK_OK) {
		fprintf(stderr, "len: stack underflow\n");
		result.code = -1;
		return result;
	}

	if (elem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "len: expected array (ptr), got %d\n", elem.type);
		result.code = -1;
		return result;
	}

	qd_array_t* arr = (qd_array_t*)elem.value.p;
	if (!arr) {
		fprintf(stderr, "len: null array\n");
		result.code = -1;
		return result;
	}

	size_t len = arr->length;
	qd_array_release(arr); // Release the consumed array
	qd_stack_push_int(ctx->st, (int64_t)len);
	return result;
}

qd_exec_result qd_nth(qd_context* ctx) {
	qd_exec_result result = {0};
	if (!ctx || !ctx->st) {
		result.code = -1;
		return result;
	}

	// Pop index
	qd_stack_element_t indexElem;
	if (qd_stack_pop(ctx->st, &indexElem) != QD_STACK_OK) {
		fprintf(stderr, "nth: stack underflow (index)\n");
		result.code = -1;
		return result;
	}

	if (indexElem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "nth: expected integer index, got %d\n", indexElem.type);
		result.code = -1;
		return result;
	}

	int64_t index = indexElem.value.i;

	// Pop array
	qd_stack_element_t arrElem;
	if (qd_stack_pop(ctx->st, &arrElem) != QD_STACK_OK) {
		fprintf(stderr, "nth: stack underflow (array)\n");
		result.code = -1;
		return result;
	}

	if (arrElem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "nth: expected array (ptr), got %d\n", arrElem.type);
		result.code = -1;
		return result;
	}

	qd_array_t* arr = (qd_array_t*)arrElem.value.p;
	if (!arr) {
		fprintf(stderr, "nth: null array\n");
		result.code = -1;
		return result;
	}

	if (index < 0 || (size_t)index >= arr->length) {
		fprintf(stderr, "nth: index %ld out of bounds (length %zu)\n", index, arr->length);
		qd_array_release(arr); // Release before error return
		result.code = -1;
		return result;
	}

	// Push element based on array type
	switch (arr->elemType) {
	case QD_ARRAY_TYPE_INT:
		qd_stack_push_int(ctx->st, arr->data.i[index]);
		break;
	case QD_ARRAY_TYPE_FLOAT:
		qd_stack_push_float(ctx->st, arr->data.f[index]);
		break;
	case QD_ARRAY_TYPE_STR: {
		// For strings, we need to retain and push
		qd_string_t* str = (qd_string_t*)arr->data.p[index];
		if (str) {
			qd_string_retain(str);
			// Push string directly - the stack will take ownership
			qd_stack_element_t strElem;
			strElem.type = QD_STACK_TYPE_STR;
			strElem.value.s = str;
			strElem.is_error_tainted = false;
			// Push directly to stack
			if (ctx->st->size >= ctx->st->capacity) {
				fprintf(stderr, "nth: stack overflow\n");
				qd_string_release(str);
				qd_array_release(arr); // Release before error return
				result.code = -1;
				return result;
			}
			ctx->st->data[ctx->st->size++] = strElem;
		} else {
			// Push empty string
			qd_stack_push_str(ctx->st, "");
		}
		break;
	}
	case QD_ARRAY_TYPE_PTR:
		qd_stack_push_ptr(ctx->st, arr->data.p[index]);
		break;
	default:
		fprintf(stderr, "nth: unknown array type %d\n", arr->elemType);
		qd_array_release(arr); // Release before error return
		result.code = -1;
		return result;
	}

	qd_array_release(arr); // Release the consumed array
	return result;
}
