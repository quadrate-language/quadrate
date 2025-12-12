/**
 * @file array.c
 * @brief Dynamic array implementation for Quadrate runtime
 */

#include <qdrt/array.h>
#include <qdrt/qd_string.h>
#include <qdrt/qd_struct.h>
#include <qdrt/stack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ptr_registry.h"

// Global registry of valid array pointers
// Note: We use a pointer registry rather than magic numbers because
// qd_ptr_retain/release can receive any pointer, including raw memory
// from mem::alloc. Reading uninitialized memory for magic number check
// causes valgrind errors.
static ptr_registry_t array_registry = PTR_REGISTRY_INITIALIZER;

int qd_array_is_valid(const void* ptr) {
	if (!ptr) {
		return 0;
	}
	return ptr_registry_contains(&array_registry, ptr);
}

qd_array_t* qd_array_create(size_t capacity, qd_array_type elemType) {
	if (capacity == 0) {
		capacity = QD_ARRAY_DEFAULT_CAPACITY;
	}

	qd_array_t* arr = (qd_array_t*)malloc(sizeof(qd_array_t));
	if (!arr) {
		return NULL;
	}

	arr->magic = QD_ARRAY_MAGIC;
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

	// Register this array pointer for validation
	ptr_registry_add(&array_registry, arr);

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
		// Release all struct references (safe for non-struct pointers via registry check)
		for (size_t i = 0; i < arr->length; i++) {
			if (arr->data.p[i]) {
				qd_struct_release(arr->data.p[i]);
			}
		}
		free(arr->data.p);
		break;
	}

	// Unregister from the array registry
	ptr_registry_remove(&array_registry, arr);

	// Clear magic to prevent double-free detection
	arr->magic = 0;
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
	if (newCapacity < QD_ARRAY_DEFAULT_CAPACITY) {
		newCapacity = QD_ARRAY_DEFAULT_CAPACITY;
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

qd_exec_result qd_makei(qd_context* ctx) {
	qd_exec_result result = {0};
	if (!ctx || !ctx->st) {
		result.code = -1;
		return result;
	}

	qd_stack_element_t sizeElem;
	if (qd_stack_pop(ctx->st, &sizeElem) != QD_STACK_OK) {
		fprintf(stderr, "makei: stack underflow\n");
		result.code = -1;
		return result;
	}

	if (sizeElem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "makei: expected integer size, got %d\n", sizeElem.type);
		result.code = -1;
		return result;
	}

	int64_t size = sizeElem.value.i;
	if (size < 0) {
		fprintf(stderr, "makei: negative size %ld\n", size);
		result.code = -1;
		return result;
	}

	qd_array_t* arr = qd_array_create((size_t)size, QD_ARRAY_TYPE_INT);
	if (!arr) {
		fprintf(stderr, "makei: allocation failed\n");
		result.code = -1;
		return result;
	}

	// Initialize elements to 0
	for (size_t i = 0; i < (size_t)size; i++) {
		arr->data.i[i] = 0;
	}
	arr->length = (size_t)size;

	qd_stack_push_ptr(ctx->st, arr);
	return result;
}

qd_exec_result qd_makef(qd_context* ctx) {
	qd_exec_result result = {0};
	if (!ctx || !ctx->st) {
		result.code = -1;
		return result;
	}

	qd_stack_element_t sizeElem;
	if (qd_stack_pop(ctx->st, &sizeElem) != QD_STACK_OK) {
		fprintf(stderr, "makef: stack underflow\n");
		result.code = -1;
		return result;
	}

	if (sizeElem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "makef: expected integer size, got %d\n", sizeElem.type);
		result.code = -1;
		return result;
	}

	int64_t size = sizeElem.value.i;
	if (size < 0) {
		fprintf(stderr, "makef: negative size %ld\n", size);
		result.code = -1;
		return result;
	}

	qd_array_t* arr = qd_array_create((size_t)size, QD_ARRAY_TYPE_FLOAT);
	if (!arr) {
		fprintf(stderr, "makef: allocation failed\n");
		result.code = -1;
		return result;
	}

	// Initialize elements to 0.0
	for (size_t i = 0; i < (size_t)size; i++) {
		arr->data.f[i] = 0.0;
	}
	arr->length = (size_t)size;

	qd_stack_push_ptr(ctx->st, arr);
	return result;
}

qd_exec_result qd_makes(qd_context* ctx) {
	qd_exec_result result = {0};
	if (!ctx || !ctx->st) {
		result.code = -1;
		return result;
	}

	qd_stack_element_t sizeElem;
	if (qd_stack_pop(ctx->st, &sizeElem) != QD_STACK_OK) {
		fprintf(stderr, "makes: stack underflow\n");
		result.code = -1;
		return result;
	}

	if (sizeElem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "makes: expected integer size, got %d\n", sizeElem.type);
		result.code = -1;
		return result;
	}

	int64_t size = sizeElem.value.i;
	if (size < 0) {
		fprintf(stderr, "makes: negative size %ld\n", size);
		result.code = -1;
		return result;
	}

	qd_array_t* arr = qd_array_create((size_t)size, QD_ARRAY_TYPE_STR);
	if (!arr) {
		fprintf(stderr, "makes: allocation failed\n");
		result.code = -1;
		return result;
	}

	// Initialize elements to empty strings
	for (size_t i = 0; i < (size_t)size; i++) {
		arr->data.p[i] = qd_string_create("");
	}
	arr->length = (size_t)size;

	qd_stack_push_ptr(ctx->st, arr);
	return result;
}

qd_exec_result qd_makep(qd_context* ctx) {
	qd_exec_result result = {0};
	if (!ctx || !ctx->st) {
		result.code = -1;
		return result;
	}

	qd_stack_element_t sizeElem;
	if (qd_stack_pop(ctx->st, &sizeElem) != QD_STACK_OK) {
		fprintf(stderr, "makep: stack underflow\n");
		result.code = -1;
		return result;
	}

	if (sizeElem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "makep: expected integer size, got %d\n", sizeElem.type);
		result.code = -1;
		return result;
	}

	int64_t size = sizeElem.value.i;
	if (size < 0) {
		fprintf(stderr, "makep: negative size %ld\n", size);
		result.code = -1;
		return result;
	}

	qd_array_t* arr = qd_array_create((size_t)size, QD_ARRAY_TYPE_PTR);
	if (!arr) {
		fprintf(stderr, "makep: allocation failed\n");
		result.code = -1;
		return result;
	}

	// Initialize elements to null (already done by qd_array_create for PTR type)
	arr->length = (size_t)size;

	qd_stack_push_ptr(ctx->st, arr);
	return result;
}

qd_exec_result qd_append(qd_context* ctx) {
	qd_exec_result result = {0};
	if (!ctx || !ctx->st) {
		result.code = -1;
		return result;
	}

	// Pop value first (top of stack)
	qd_stack_element_t valueElem;
	if (qd_stack_pop(ctx->st, &valueElem) != QD_STACK_OK) {
		fprintf(stderr, "append: stack underflow (value)\n");
		result.code = -1;
		return result;
	}

	// Pop array
	qd_stack_element_t arrElem;
	if (qd_stack_pop(ctx->st, &arrElem) != QD_STACK_OK) {
		fprintf(stderr, "append: stack underflow (array)\n");
		// Clean up value if it's a string
		if (valueElem.type == QD_STACK_TYPE_STR) {
			qd_string_release(valueElem.value.s);
		}
		result.code = -1;
		return result;
	}

	if (arrElem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "append: expected array (ptr), got %d\n", arrElem.type);
		if (valueElem.type == QD_STACK_TYPE_STR) {
			qd_string_release(valueElem.value.s);
		}
		result.code = -1;
		return result;
	}

	qd_array_t* arr = (qd_array_t*)arrElem.value.p;
	if (!arr) {
		fprintf(stderr, "append: null array\n");
		if (valueElem.type == QD_STACK_TYPE_STR) {
			qd_string_release(valueElem.value.s);
		}
		result.code = -1;
		return result;
	}

	// Append based on array type
	int appendResult = -1;
	switch (arr->elemType) {
	case QD_ARRAY_TYPE_INT:
		if (valueElem.type == QD_STACK_TYPE_INT) {
			appendResult = qd_array_push_int(arr, valueElem.value.i);
		} else {
			fprintf(stderr, "append: type mismatch - int array, got type %d\n", valueElem.type);
		}
		break;
	case QD_ARRAY_TYPE_FLOAT:
		if (valueElem.type == QD_STACK_TYPE_FLOAT) {
			appendResult = qd_array_push_float(arr, valueElem.value.f);
		} else if (valueElem.type == QD_STACK_TYPE_INT) {
			// Allow int to float conversion
			appendResult = qd_array_push_float(arr, (double)valueElem.value.i);
		} else {
			fprintf(stderr, "append: type mismatch - float array, got type %d\n", valueElem.type);
		}
		break;
	case QD_ARRAY_TYPE_STR:
		if (valueElem.type == QD_STACK_TYPE_STR) {
			// Push the string directly (push_ptr will retain it)
			appendResult = qd_array_push_ptr(arr, valueElem.value.s);
			// Don't release - array now owns it
		} else {
			fprintf(stderr, "append: type mismatch - string array, got type %d\n", valueElem.type);
			if (valueElem.type == QD_STACK_TYPE_STR) {
				qd_string_release(valueElem.value.s);
			}
		}
		break;
	case QD_ARRAY_TYPE_PTR:
		if (valueElem.type == QD_STACK_TYPE_PTR) {
			appendResult = qd_array_push_ptr(arr, valueElem.value.p);
		} else {
			fprintf(stderr, "append: type mismatch - pointer array, got type %d\n", valueElem.type);
		}
		break;
	default:
		fprintf(stderr, "append: unknown array type %d\n", arr->elemType);
		break;
	}

	if (appendResult != 0) {
		qd_array_release(arr);
		result.code = -1;
		return result;
	}

	// Push array back onto stack (don't release, caller still owns it)
	qd_stack_push_ptr(ctx->st, arr);
	return result;
}

qd_exec_result qd_set(qd_context* ctx) {
	qd_exec_result result = {0};
	if (!ctx || !ctx->st) {
		result.code = -1;
		return result;
	}

	// Pop value first (top of stack)
	qd_stack_element_t valueElem;
	if (qd_stack_pop(ctx->st, &valueElem) != QD_STACK_OK) {
		fprintf(stderr, "set: stack underflow (value)\n");
		result.code = -1;
		return result;
	}

	// Pop index
	qd_stack_element_t indexElem;
	if (qd_stack_pop(ctx->st, &indexElem) != QD_STACK_OK) {
		fprintf(stderr, "set: stack underflow (index)\n");
		if (valueElem.type == QD_STACK_TYPE_STR) {
			qd_string_release(valueElem.value.s);
		}
		result.code = -1;
		return result;
	}

	if (indexElem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "set: expected integer index, got %d\n", indexElem.type);
		if (valueElem.type == QD_STACK_TYPE_STR) {
			qd_string_release(valueElem.value.s);
		}
		result.code = -1;
		return result;
	}

	// Pop array
	qd_stack_element_t arrElem;
	if (qd_stack_pop(ctx->st, &arrElem) != QD_STACK_OK) {
		fprintf(stderr, "set: stack underflow (array)\n");
		if (valueElem.type == QD_STACK_TYPE_STR) {
			qd_string_release(valueElem.value.s);
		}
		result.code = -1;
		return result;
	}

	if (arrElem.type != QD_STACK_TYPE_PTR) {
		fprintf(stderr, "set: expected array (ptr), got %d\n", arrElem.type);
		if (valueElem.type == QD_STACK_TYPE_STR) {
			qd_string_release(valueElem.value.s);
		}
		result.code = -1;
		return result;
	}

	qd_array_t* arr = (qd_array_t*)arrElem.value.p;
	if (!arr) {
		fprintf(stderr, "set: null array\n");
		if (valueElem.type == QD_STACK_TYPE_STR) {
			qd_string_release(valueElem.value.s);
		}
		result.code = -1;
		return result;
	}

	int64_t index = indexElem.value.i;
	if (index < 0 || (size_t)index >= arr->length) {
		fprintf(stderr, "set: index %ld out of bounds (length %zu)\n", index, arr->length);
		if (valueElem.type == QD_STACK_TYPE_STR) {
			qd_string_release(valueElem.value.s);
		}
		qd_array_release(arr);
		result.code = -1;
		return result;
	}

	// Set based on array type
	int setResult = -1;
	switch (arr->elemType) {
	case QD_ARRAY_TYPE_INT:
		if (valueElem.type == QD_STACK_TYPE_INT) {
			setResult = qd_array_set_int(arr, (size_t)index, valueElem.value.i);
		} else {
			fprintf(stderr, "set: type mismatch - int array, got type %d\n", valueElem.type);
		}
		break;
	case QD_ARRAY_TYPE_FLOAT:
		if (valueElem.type == QD_STACK_TYPE_FLOAT) {
			setResult = qd_array_set_float(arr, (size_t)index, valueElem.value.f);
		} else if (valueElem.type == QD_STACK_TYPE_INT) {
			// Allow int to float conversion
			setResult = qd_array_set_float(arr, (size_t)index, (double)valueElem.value.i);
		} else {
			fprintf(stderr, "set: type mismatch - float array, got type %d\n", valueElem.type);
		}
		break;
	case QD_ARRAY_TYPE_STR:
		if (valueElem.type == QD_STACK_TYPE_STR) {
			// Release old string and set new one
			if (arr->data.p[index]) {
				qd_string_release((qd_string_t*)arr->data.p[index]);
			}
			arr->data.p[index] = valueElem.value.s;
			// Don't release - array now owns it
			setResult = 0;
		} else {
			fprintf(stderr, "set: type mismatch - string array, got type %d\n", valueElem.type);
			if (valueElem.type == QD_STACK_TYPE_STR) {
				qd_string_release(valueElem.value.s);
			}
		}
		break;
	case QD_ARRAY_TYPE_PTR:
		if (valueElem.type == QD_STACK_TYPE_PTR) {
			setResult = qd_array_set_ptr(arr, (size_t)index, valueElem.value.p);
		} else {
			fprintf(stderr, "set: type mismatch - pointer array, got type %d\n", valueElem.type);
		}
		break;
	default:
		fprintf(stderr, "set: unknown array type %d\n", arr->elemType);
		break;
	}

	qd_array_release(arr);

	if (setResult != 0) {
		result.code = -1;
		return result;
	}

	return result;
}
