/**
 * @file array.c
 * @brief Dynamic array implementation for Quadrate runtime
 */

#include "runtime_internal.h"
#include <inttypes.h>
#include <quadrate/rt/array.h>
#include <quadrate/rt/qd_string.h>
#include <quadrate/rt/qd_struct.h>
#include <quadrate/rt/stack.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// An array is known by the slot or field that holds it, never looked up: there is no registry
// of them. This checks the magic of a pointer that is an array or null, and a freed array's is
// cleared before it goes.
int qd_array_is_valid(const void* ptr) {
	return ptr != NULL && ((const qd_array_t*)ptr)->magic == QD_ARRAY_MAGIC;
}

// An untyped array becoming a d128 one. Its pointer-sized buffer holds nothing yet but
// zeros, and a d128 is twice as wide, so it is replaced rather than reinterpreted. Zeroed
// bits are a d128 zero, as they are an int or float one.
static int adopt_d128(qd_array_t* arr) {
	qd_d128_t* data = (qd_d128_t*)calloc(arr->capacity > 0 ? arr->capacity : 1, sizeof(qd_d128_t));
	if (!data) {
		return -1;
	}
	free(arr->data.p);
	arr->data.d = data;
	arr->elemType = QD_ARRAY_TYPE_D128;
	return 0;
}

qd_array_t* qd_array_create(size_t capacity, qd_array_type elemType) {
	if (capacity == 0) {
		capacity = QD_ARRAY_DEFAULT_CAPACITY;
	}

	// Guard against overflow in the `capacity * elemSize` allocations below.
	// The largest element size is 8 bytes (int64/double/ptr), so bounding
	// against that is sufficient for every branch.
	if (capacity > SIZE_MAX / sizeof(int64_t)) {
		return NULL;
	}

	qd_array_t* arr = (qd_array_t*)malloc(sizeof(qd_array_t));
	if (!arr) {
		return NULL;
	}

	arr->magic = QD_ARRAY_MAGIC;
	atomic_init(&arr->refcount, 1);
	arr->length = 0;
	arr->capacity = capacity;
	arr->elemType = elemType;
	arr->elemKind = 0;

	// Allocate data based on element type
	size_t elemSize;
	switch (elemType) {
	case QD_ARRAY_TYPE_INT:
		elemSize = sizeof(int64_t);
		arr->data.i = (int64_t*)calloc(capacity, elemSize);
		if (!arr->data.i) {
			free(arr);
			return NULL;
		}
		break;
	case QD_ARRAY_TYPE_FLOAT:
		elemSize = sizeof(double);
		arr->data.f = (double*)calloc(capacity, elemSize);
		if (!arr->data.f) {
			free(arr);
			return NULL;
		}
		break;
	case QD_ARRAY_TYPE_D128:
		// Zeroed bits are a d128 zero, which is what a slot nothing has been written to holds
		arr->data.d = (qd_d128_t*)calloc(capacity > 0 ? capacity : 1, sizeof(qd_d128_t));
		if (!arr->data.d) {
			free(arr);
			return NULL;
		}
		break;
	case QD_ARRAY_TYPE_STR:
	case QD_ARRAY_TYPE_PTR:
	case QD_ARRAY_TYPE_ANY:
		// These are all 8 bytes, so an as-yet-untyped array can allocate as pointers and be
		// reinterpreted once its type is known, or widened if it turns out to hold d128.
		// Zeroing matters for the string and pointer cases, which release what they hold.
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

// One more reference. A pointer whose magic is not a live array's is left alone, as
// qd_array_release leaves it.
void qd_array_retain(qd_array_t* arr) {
	if (qd_array_is_valid(arr)) {
		atomic_fetch_add(&arr->refcount, 1);
	}
}

static void array_free_last(qd_array_t* arr);

void qd_array_release(qd_array_t* arr) {
	if (!arr) {
		return;
	}

	// A freed array's magic is cleared, so one released again is left alone
	if (!qd_array_is_valid(arr)) {
		return;
	}

	size_t old_count = atomic_fetch_sub(&arr->refcount, 1);
	if (old_count == 0) {
		fprintf(stderr, "Fatal error: free called on already-freed array\n");
		qdrt_fatal_exit();
	}
	if (old_count > 1) {
		return;
	}

	array_free_last(arr);
}

// Its last reference gone: its contents, its registration, itself
// An element of a pointer array, counted by the kind the array holds
static void elem_retain(const qd_array_t* arr, void* p) {
	if (p == NULL) {
		return;
	}
	switch (arr->elemKind) {
	case QD_PTR_RAW:
		break;
	case QD_PTR_STRUCT:
		qd_struct_retain(p);
		break;
	case QD_PTR_ARRAY:
		qd_array_retain(p);
		break;
	default:
		break; // raw memory
	}
}

static void elem_release(const qd_array_t* arr, void* p) {
	if (p == NULL) {
		return;
	}
	switch (arr->elemKind) {
	case QD_PTR_RAW:
		break;
	case QD_PTR_STRUCT:
		qd_struct_release(p);
		break;
	case QD_PTR_ARRAY:
		qd_array_release(p);
		break;
	default:
		break; // raw memory
	}
}

// The kind of the first pointer stored is the kind of all of them
static void adopt_kind(qd_array_t* arr, const qd_stack_element_t* value) {
	if (arr->elemType == QD_ARRAY_TYPE_PTR && value->type == QD_STACK_TYPE_PTR && arr->elemKind == 0) {
		arr->elemKind = value->ptr_kind;
	}
}

static void array_free_last(qd_array_t* arr) {
	// Free contents
	switch (arr->elemType) {
	case QD_ARRAY_TYPE_ANY:
		// No element ever went in, so there is nothing to release -- but the buffer was still
		// allocated (as pointers, see qd_array_create) and still has to be freed.
		free(arr->data.p);
		break;
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
	case QD_ARRAY_TYPE_D128:
		free(arr->data.d);
		break;
	case QD_ARRAY_TYPE_PTR:
		for (size_t i = 0; i < arr->length; i++) {
			elem_release(arr, arr->data.p[i]);
		}
		free(arr->data.p);
		break;
	}

	// Unregister from the array registry

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

int qd_array_get_d128(const qd_array_t* arr, size_t index, qd_d128_t* value) {
	if (!arr || !value || index >= arr->length || arr->elemType != QD_ARRAY_TYPE_D128) {
		return -1;
	}
	*value = arr->data.d[index];
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

int qd_array_set_d128(qd_array_t* arr, size_t index, qd_d128_t value) {
	if (!arr || index >= arr->length || arr->elemType != QD_ARRAY_TYPE_D128) {
		return -1;
	}
	arr->data.d[index] = value;
	return 0;
}

int qd_array_set_ptr(qd_array_t* arr, size_t index, void* value) {
	if (!arr || index >= arr->length || (arr->elemType != QD_ARRAY_TYPE_PTR && arr->elemType != QD_ARRAY_TYPE_STR)) {
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
	// Check for integer overflow before doubling
	if (arr->capacity > SIZE_MAX / 2) {
		return -1; // Would overflow
	}
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
	case QD_ARRAY_TYPE_D128: {
		qd_d128_t* newData = (qd_d128_t*)realloc(arr->data.d, newCapacity * sizeof(qd_d128_t));
		if (!newData) {
			return -1;
		}
		memset(newData + arr->capacity, 0, (newCapacity - arr->capacity) * sizeof(qd_d128_t));
		arr->data.d = newData;
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

int qd_array_push_d128(qd_array_t* arr, qd_d128_t value) {
	if (!arr || arr->elemType != QD_ARRAY_TYPE_D128) {
		return -1;
	}
	if (arr->length >= arr->capacity) {
		if (qd_array_grow(arr) != 0) {
			return -1;
		}
	}
	arr->data.d[arr->length++] = value;
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

	// For string arrays, take a reference of the array's own
	if (arr->elemType == QD_ARRAY_TYPE_STR && value) {
		qd_string_retain((qd_string_t*)value);
	}

	arr->data.p[arr->length++] = value;
	return 0;
}

// Stack-based array operations
//
// Each raises on a bad argument, as every other builtin does. They used to print a line to
// stderr and return -1, which compiled code does not read: `args 1 nth` on a one-element
// array printed "out of bounds", pushed nothing, and the program went on to fail at
// whatever came next.

static const char* array_type_name(qd_array_type type) {
	switch (type) {
	case QD_ARRAY_TYPE_INT:
		return "i64";
	case QD_ARRAY_TYPE_FLOAT:
		return "f64";
	case QD_ARRAY_TYPE_STR:
		return "str";
	case QD_ARRAY_TYPE_PTR:
		return "ptr";
	case QD_ARRAY_TYPE_D128:
		return "d128";
	case QD_ARRAY_TYPE_ANY:
		break;
	}
	return "any";
}

// Release what the word popped -- the array's reference and the value, either may be
// absent -- and raise
_Noreturn __attribute__((format(printf, 5, 6))) static void array_raise(
		qd_context* ctx, const char* op, qd_array_t* arr, qd_stack_element_t* value, const char* fmt, ...) {
	char detail[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(detail, sizeof(detail), fmt, args);
	va_end(args);
	if (value) {
		qdrt_release_element(value);
	}
	if (arr) {
		qd_array_release(arr);
	}
	QDRT_FATAL(ctx, op, "%s", detail);
}

static qd_array_t* array_arg(qd_context* ctx, const char* op, void* p, qd_stack_element_t* value) {
	if (!p) {
		array_raise(ctx, op, NULL, value, "Null array");
	}
	return (qd_array_t*)p;
}

static void check_index(qd_context* ctx, const char* op, qd_array_t* arr, int64_t index, qd_stack_element_t* value) {
	if (index < 0 || (size_t)index >= arr->length) {
		array_raise(ctx, op, arr, value, "Index %" PRId64 " out of bounds (length %zu)", index, arr->length);
	}
}

// An array whose element type is still open takes the type of the first value written to it.
// Every slot is zeroed, which is the zero value of whichever type it becomes.
static void adopt_type(qd_context* ctx, const char* op, qd_array_t* arr, qd_stack_element_t* value) {
	if (arr->elemType != QD_ARRAY_TYPE_ANY) {
		return;
	}
	switch (value->type) {
	case QD_STACK_TYPE_INT:
		arr->elemType = QD_ARRAY_TYPE_INT;
		break;
	case QD_STACK_TYPE_FLOAT:
		arr->elemType = QD_ARRAY_TYPE_FLOAT;
		break;
	case QD_STACK_TYPE_STR:
		arr->elemType = QD_ARRAY_TYPE_STR;
		break;
	case QD_STACK_TYPE_D128:
		if (adopt_d128(arr) != 0) {
			array_raise(ctx, op, arr, value, "Out of memory");
		}
		break;
	case QD_STACK_TYPE_PTR:
		arr->elemType = QD_ARRAY_TYPE_PTR;
		break;
	}
}

// Whether `value` can go into `arr`; an i64 widens into an f64 array
static bool fits(const qd_array_t* arr, const qd_stack_element_t* value) {
	switch (arr->elemType) {
	case QD_ARRAY_TYPE_INT:
		return value->type == QD_STACK_TYPE_INT;
	case QD_ARRAY_TYPE_FLOAT:
		return value->type == QD_STACK_TYPE_FLOAT || value->type == QD_STACK_TYPE_INT;
	case QD_ARRAY_TYPE_STR:
		return value->type == QD_STACK_TYPE_STR;
	case QD_ARRAY_TYPE_PTR:
		return value->type == QD_STACK_TYPE_PTR;
	case QD_ARRAY_TYPE_D128:
		return value->type == QD_STACK_TYPE_D128;
	case QD_ARRAY_TYPE_ANY:
		break;
	}
	return false;
}

static void check_fits(qd_context* ctx, const char* op, qd_array_t* arr, qd_stack_element_t* value) {
	if (!fits(arr, value)) {
		const char* elem = array_type_name(arr->elemType);
		const char* got = qd_type_name(value->type);
		array_raise(ctx, op, arr, value, "Cannot put a %s into an array of %s", got, elem);
	}
}

// A string is a sequence of characters, so `len` and `nth` read one as they read an array: its
// length in characters and a character's codepoint, as len and strings::char_at do.
// That is what lets `each`, `fold` and the searching combinators walk a string. A string is
// read where it lies -- its length is memoised and qd_string_char_offset resumes a forward scan
// from where the last one stopped -- so walking it allocates nothing.

// The codepoint at `p`, `avail` bytes left, by the rule the strings module decodes by
// (unicode/codepoint.h, which the runtime cannot include): a malformed or truncated sequence is
// one character, the value of its lead byte.
static int64_t decode_char(const unsigned char* p, size_t avail) {
	const unsigned char lead = p[0];
	size_t need = lead < 0x80u				? 1
				  : (lead & 0xE0u) == 0xC0u ? 2
				  : (lead & 0xF0u) == 0xE0u ? 3
				  : (lead & 0xF8u) == 0xF0u ? 4
											: 1;
	if (need > avail) {
		return lead;
	}
	for (size_t k = 1; k < need; k++) {
		if ((p[k] & 0xC0u) != 0x80u) {
			return lead;
		}
	}
	if (need == 1) {
		return lead;
	}
	int64_t cp = need == 2 ? (lead & 0x1Fu) : need == 3 ? (lead & 0x0Fu) : (lead & 0x07u);
	for (size_t k = 1; k < need; k++) {
		cp = (cp << 6) | (p[k] & 0x3Fu);
	}
	return cp;
}

int qd_len(qd_context* ctx) {
	qd_stack_element_t v;
	qd_args(ctx, "len", "v", &v);
	if (v.type == QD_STACK_TYPE_STR) {
		const int64_t n = (int64_t)qd_string_char_length(v.value.s);
		qd_string_release(v.value.s);
		return qd_push_i(ctx, n);
	}
	if (v.type != QD_STACK_TYPE_PTR) {
		QDRT_FATAL(ctx, "len", "Expected an array or a string, got %s", qd_type_name(v.type));
	}
	qd_array_t* arr = array_arg(ctx, "len", v.value.p, NULL);
	const size_t len = arr->length;
	qd_array_release(arr); // Release the consumed array
	return qd_push_i(ctx, (int64_t)len);
}

static int nth_array(qd_context* ctx, void* p, int64_t index) {
	qd_array_t* arr = array_arg(ctx, "nth", p, NULL);
	check_index(ctx, "nth", arr, index, NULL);

	switch (arr->elemType) {
	case QD_ARRAY_TYPE_ANY:
		// Nothing has been written, so the element type is still open and every slot is zero
		qd_push_i(ctx, 0);
		break;
	case QD_ARRAY_TYPE_INT:
		qd_push_i(ctx, arr->data.i[index]);
		break;
	case QD_ARRAY_TYPE_FLOAT:
		qd_push_f(ctx, arr->data.f[index]);
		break;
	case QD_ARRAY_TYPE_STR: {
		qd_string_t* str = (qd_string_t*)arr->data.p[index];
		if (str) {
			qd_push_s_ref(ctx, str);
		} else {
			qd_push_s(ctx, "");
		}
		break;
	}
	case QD_ARRAY_TYPE_D128:
		qd_push_d(ctx, arr->data.d[index]);
		break;
	case QD_ARRAY_TYPE_PTR: {
		void* ptr = arr->data.p[index];
		elem_retain(arr, ptr);
		qd_push_ptr_kind(ctx, ptr, arr->elemKind);
		break;
	}
	}

	qd_array_release(arr); // Release the consumed array
	return 0;
}

int64_t qd_string_nth(qd_context* ctx, qd_string_t* s, int64_t index) {
	const size_t n = qd_string_char_length(s);
	if (index < 0 || (size_t)index >= n) {
		QDRT_FATAL(ctx, "nth", "Index %" PRId64 " out of bounds (length %zu)", index, n);
	}
	const size_t off = qd_string_char_offset(s, (size_t)index);
	return decode_char((const unsigned char*)s->data + off, s->length - off);
}

int64_t qd_string_len(qd_string_t* s) {
	return (int64_t)qd_string_char_length(s);
}

int qd_nth(qd_context* ctx) {
	qd_stack_element_t v;
	int64_t index;
	qd_args(ctx, "nth", "vi", &v, &index);
	if (v.type == QD_STACK_TYPE_STR) {
		qd_string_t* s = v.value.s;
		const size_t n = qd_string_char_length(s);
		if (index < 0 || (size_t)index >= n) {
			qd_string_release(s);
			QDRT_FATAL(ctx, "nth", "Index %" PRId64 " out of bounds (length %zu)", index, n);
		}
		const size_t off = qd_string_char_offset(s, (size_t)index);
		const int64_t c = decode_char((const unsigned char*)s->data + off, s->length - off);
		qd_string_release(s);
		return qd_push_i(ctx, c);
	}
	if (v.type != QD_STACK_TYPE_PTR) {
		QDRT_FATAL(ctx, "nth", "Expected an array or a string, got %s", qd_type_name(v.type));
	}
	return nth_array(ctx, v.value.p, index);
}

// clone on an array: a new array of the same elements. Shallow, as cloning a struct is: a
// string or a struct an element refers to is shared, one reference more, not copied.
int qd_clone_array(qd_context* ctx) {
	void* p;
	qd_args(ctx, "clone", "p", &p);
	qd_array_t* src = array_arg(ctx, "clone", p, NULL);
	qd_array_t* copy = qd_array_create(src->length, src->elemType);
	if (!copy) {
		qd_array_release(src);
		QDRT_FATAL(ctx, "clone", "Out of memory");
	}
	copy->elemKind = src->elemKind;
	switch (src->elemType) {
	case QD_ARRAY_TYPE_INT:
		memcpy(copy->data.i, src->data.i, src->length * sizeof(int64_t));
		break;
	case QD_ARRAY_TYPE_FLOAT:
		memcpy(copy->data.f, src->data.f, src->length * sizeof(double));
		break;
	case QD_ARRAY_TYPE_D128:
		memcpy(copy->data.d, src->data.d, src->length * sizeof(qd_d128_t));
		break;
	case QD_ARRAY_TYPE_STR:
		for (size_t i = 0; i < src->length; i++) {
			copy->data.p[i] = src->data.p[i] ? qd_string_retain((qd_string_t*)src->data.p[i]) : NULL;
		}
		break;
	case QD_ARRAY_TYPE_PTR:
		for (size_t i = 0; i < src->length; i++) {
			void* elem = src->data.p[i];
			elem_retain(src, elem);
			copy->data.p[i] = elem;
		}
		break;
	case QD_ARRAY_TYPE_ANY:
		// Nothing written yet, so every slot is still zero
		break;
	}
	copy->length = src->length;
	qd_array_release(src);
	return qd_push_array(ctx, copy);
}

// The `make` words: an array of `n` zero values of one type
static int make_array(qd_context* ctx, const char* op, qd_array_type type) {
	int64_t size;
	qd_args(ctx, op, "i", &size);
	if (size < 0) {
		QDRT_FATAL(ctx, op, "Negative size %" PRId64, size);
	}
	qd_array_t* arr = qd_array_create((size_t)size, type);
	if (!arr) {
		QDRT_FATAL(ctx, op, "Out of memory");
	}
	// qd_array_create zeroes the slots: 0, 0.0, a d128 zero and null. A string array
	// starts with empty strings instead of nulls.
	if (type == QD_ARRAY_TYPE_STR) {
		for (size_t i = 0; i < (size_t)size; i++) {
			arr->data.p[i] = qd_string_create("");
		}
	}
	arr->length = (size_t)size;
	return qd_push_array(ctx, arr);
}

int qd_makei(qd_context* ctx) {
	return make_array(ctx, "makei", QD_ARRAY_TYPE_INT);
}

int qd_makef(qd_context* ctx) {
	return make_array(ctx, "makef", QD_ARRAY_TYPE_FLOAT);
}

int qd_makes(qd_context* ctx) {
	return make_array(ctx, "makes", QD_ARRAY_TYPE_STR);
}

int qd_makep(qd_context* ctx) {
	return make_array(ctx, "makep", QD_ARRAY_TYPE_PTR);
}

int qd_maked(qd_context* ctx) {
	return make_array(ctx, "maked", QD_ARRAY_TYPE_D128);
}

// Creates an array whose element type is decided by the first value written to it. Used for
// `make<T>` inside a generic function, where T is erased at run time.
int qd_makea(qd_context* ctx) {
	return make_array(ctx, "makea", QD_ARRAY_TYPE_ANY);
}

int qd_append(qd_context* ctx) {
	void* p;
	qd_stack_element_t value;
	qd_args(ctx, "append", "pv", &p, &value);
	qd_array_t* arr = array_arg(ctx, "append", p, &value);
	adopt_type(ctx, "append", arr, &value);
	adopt_kind(arr, &value);
	check_fits(ctx, "append", arr, &value);

	int appended = 0;
	switch (arr->elemType) {
	case QD_ARRAY_TYPE_INT:
		appended = qd_array_push_int(arr, value.value.i);
		break;
	case QD_ARRAY_TYPE_FLOAT:
		appended = qd_array_push_float(arr, value.type == QD_STACK_TYPE_INT ? (double)value.value.i : value.value.f);
		break;
	case QD_ARRAY_TYPE_STR:
		// The array takes a reference of its own; ours is released below
		appended = qd_array_push_ptr(arr, value.value.s);
		break;
	case QD_ARRAY_TYPE_PTR:
		appended = qd_array_push_ptr(arr, value.value.p);
		break;
	case QD_ARRAY_TYPE_D128:
		appended = qd_array_push_d128(arr, value.value.d);
		break;
	case QD_ARRAY_TYPE_ANY:
		break;
	}
	if (appended != 0) {
		array_raise(ctx, "append", arr, &value, "Out of memory");
	}
	if (value.type == QD_STACK_TYPE_STR) {
		qd_string_release(value.value.s);
	}

	// The array goes back with the reference it came with
	return qd_push_array(ctx, arr);
}

int qd_set(qd_context* ctx) {
	void* p;
	int64_t index;
	qd_stack_element_t value;
	qd_args(ctx, "set", "piv", &p, &index, &value);
	qd_array_t* arr = array_arg(ctx, "set", p, &value);
	check_index(ctx, "set", arr, index, &value);
	adopt_type(ctx, "set", arr, &value);
	adopt_kind(arr, &value);
	check_fits(ctx, "set", arr, &value);

	switch (arr->elemType) {
	case QD_ARRAY_TYPE_INT:
		qd_array_set_int(arr, (size_t)index, value.value.i);
		break;
	case QD_ARRAY_TYPE_FLOAT:
		qd_array_set_float(arr, (size_t)index, value.type == QD_STACK_TYPE_INT ? (double)value.value.i : value.value.f);
		break;
	case QD_ARRAY_TYPE_STR:
		// The array takes over the stack's reference
		if (arr->data.p[index]) {
			qd_string_release((qd_string_t*)arr->data.p[index]);
		}
		arr->data.p[index] = value.value.s;
		break;
	case QD_ARRAY_TYPE_PTR:
		// The array takes over the stack's reference, and gives back the one it held. It did not
		// give that one back, so every element `set` replaced was leaked.
		elem_release(arr, arr->data.p[index]);
		arr->data.p[index] = value.value.p;
		break;
	case QD_ARRAY_TYPE_D128:
		qd_array_set_d128(arr, (size_t)index, value.value.d);
		break;
	case QD_ARRAY_TYPE_ANY:
		break;
	}

	qd_array_release(arr); // Release the consumed array
	return 0;
}
