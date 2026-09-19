// mem_heap.c — heap-using mem functions: alloc, realloc, alloc_aligned,
// free (it's also a heap fn — it's the partner to alloc), to_string,
// from_string. These are physically separate from mem.c so the
// freestanding build can omit them entirely. Linking this file into a
// freestanding binary would drag in malloc/realloc/aligned_alloc/free
// from libc, which is exactly what we want to avoid.

#define _POSIX_C_SOURCE 200809L
#include <quadrate/mem/mem.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <stdlib.h>
#include <string.h>

#define MEM_ERR_OK 1
#define MEM_ERR_ALLOC 2
#define MEM_ERR_INVALID_ARG 3

// A fallible import reports failure through ctx->error_code: the `!` and `?` operators read that
// (via has_error or a non-zero code), not the status this function pushes. Setting only the pushed
// status made `switch` work while `!` silently did nothing -- the call fell through, the status was
// popped as if it were the result, and the next binding reported "Stack underflow when assigning to
// local variable". So every failure path here sets the code and a message as well as pushing.
#define MEM_SET_ERR(ctx, code, msg) do {                                                                              \
	(ctx)->error_code = (code);                                                                                       \
	qd_set_error_msg(ctx, msg);                                                                                       \
} while (0)

#define MEM_FAIL(ctx, code, msg) do {                                                                                 \
	MEM_SET_ERR(ctx, code, msg);                                                                                      \
	qd_push_i(ctx, code);                                                                                             \
	return (int){code};                                                                                               \
} while (0)

// Helpers duplicated from mem.c (they're tiny and we want this file to
// stand on its own without touching the safe-subset compilation unit).
static qd_stack_error pop_int(qd_context* ctx, int64_t* value) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		return err;
	}
	if (elem.type != QD_STACK_TYPE_INT) {
		return QD_STACK_ERR_TYPE_MISMATCH;
	}
	*value = elem.value.i;
	return QD_STACK_OK;
}

static qd_stack_error pop_ptr(qd_context* ctx, void** value) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		return err;
	}
	if (elem.type != QD_STACK_TYPE_PTR) {
		return QD_STACK_ERR_TYPE_MISMATCH;
	}
	*value = elem.value.p;
	return QD_STACK_OK;
}

/* Memory allocation */
int usr_mem_alloc(qd_context* ctx) {
	int64_t bytes;
	if (pop_int(ctx, &bytes) != QD_STACK_OK) {
		MEM_FAIL(ctx, MEM_ERR_INVALID_ARG, "mem::alloc: expected a byte count");
	}

	if (bytes < 0) {
		MEM_FAIL(ctx, MEM_ERR_INVALID_ARG, "mem::alloc: negative byte count");
	}

	void* ptr = malloc((size_t)bytes);
	if (ptr == NULL && bytes > 0) {
		MEM_FAIL(ctx, MEM_ERR_ALLOC, "mem::alloc: out of memory");
	}

	qd_push_p(ctx, ptr);
	qd_push_i(ctx, MEM_ERR_OK);
	return (int){0};
}

/* Reallocate memory */
int usr_mem_realloc(qd_context* ctx) {
	int64_t new_bytes;
	void* ptr;

	if (pop_int(ctx, &new_bytes) != QD_STACK_OK) {
		MEM_FAIL(ctx, MEM_ERR_INVALID_ARG, "mem::realloc: expected a byte count");
	}

	if (pop_ptr(ctx, &ptr) != QD_STACK_OK) {
		MEM_FAIL(ctx, MEM_ERR_INVALID_ARG, "mem::realloc: expected a pointer");
	}

	if (new_bytes < 0) {
		MEM_FAIL(ctx, MEM_ERR_INVALID_ARG, "mem::realloc: negative byte count");
	}

	void* new_ptr = realloc(ptr, (size_t)new_bytes);
	if (new_ptr == NULL && new_bytes > 0) {
		MEM_FAIL(ctx, MEM_ERR_ALLOC, "mem::realloc: out of memory");
	}

	qd_push_p(ctx, new_ptr);
	qd_push_i(ctx, MEM_ERR_OK);
	return (int){0};
}

/* Allocate aligned memory */
int usr_mem_alloc_aligned(qd_context* ctx) {
	int64_t bytes;
	int64_t alignment;

	if (pop_int(ctx, &bytes) != QD_STACK_OK) {
		MEM_FAIL(ctx, MEM_ERR_INVALID_ARG, "mem::alloc_aligned: expected a byte count");
	}

	if (pop_int(ctx, &alignment) != QD_STACK_OK) {
		MEM_FAIL(ctx, MEM_ERR_INVALID_ARG, "mem::alloc_aligned: expected an alignment");
	}

	if (bytes < 0 || alignment < 1) {
		MEM_FAIL(ctx, MEM_ERR_INVALID_ARG, "mem::alloc_aligned: negative size or alignment below 1");
	}

	if ((alignment & (alignment - 1)) != 0) {
		MEM_FAIL(ctx, MEM_ERR_INVALID_ARG, "mem::alloc_aligned: alignment is not a power of two");
	}

	size_t align = (size_t)alignment;
	if (align < sizeof(void*)) {
		align = sizeof(void*);
	}
	size_t size = (size_t)bytes;
	if (size % align != 0) {
		size = ((size / align) + 1) * align;
	}

	void* ptr = aligned_alloc(align, size);
	if (ptr == NULL && bytes > 0) {
		MEM_FAIL(ctx, MEM_ERR_ALLOC, "mem::alloc_aligned: out of memory");
	}

	qd_push_p(ctx, ptr);
	qd_push_i(ctx, MEM_ERR_OK);
	return (int){0};
}

/* Convert buffer to string */
int usr_mem_to_string(qd_context* ctx) {
	int64_t length;
	void* buffer;

	if (pop_int(ctx, &length) != QD_STACK_OK ||
			pop_ptr(ctx, &buffer) != QD_STACK_OK) {
		return (int){-1};
	}

	if (buffer == NULL) {
		MEM_SET_ERR(ctx, MEM_ERR_INVALID_ARG, "mem::to_string: null pointer");
		return (int){-1};
	}

	if (length < 0) {
		MEM_SET_ERR(ctx, MEM_ERR_INVALID_ARG, "mem::to_string: negative length");
		return (int){-1};
	}

	char* str = malloc((size_t)length + 1);
	if (!str) {
		MEM_SET_ERR(ctx, MEM_ERR_ALLOC, "mem::to_string: allocation failed");
		return (int){-1};
	}

	memcpy(str, buffer, (size_t)length);
	str[length] = '\0';

	int result = qd_push_s(ctx, str);
	free(str);

	return result;
}

/* Convert string to buffer */
int usr_mem_from_string(qd_context* ctx) {
	qd_stack_element_t str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		return (int){-1};
	}
	if (str_elem.type != QD_STACK_TYPE_STR) {
		qd_string_release(str_elem.value.s);
		return (int){-1};
	}

	const char* str = qd_string_data(str_elem.value.s);
	size_t length = strlen(str);

	void* buffer = malloc(length);
	if (!buffer) {
		qd_string_release(str_elem.value.s);
		MEM_SET_ERR(ctx, MEM_ERR_ALLOC, "mem::from_string: allocation failed");
		return (int){-1};
	}

	memcpy(buffer, str, length);
	qd_string_release(str_elem.value.s);

	qd_push_p(ctx, buffer);
	return qd_push_i(ctx, (int64_t)length);
}

// --- Tagged slots: storing a value of any type in raw memory ---
//
// set_i64 and its siblings each know what they are moving. A container written over a type
// parameter does not: `Vec<T>` has one body for every T, and no way to ask what T is, so it
// stored everything through set_i64 and read everything back through get_i64. That is why
// Vec<f64> and Vec<str> read back 0 and Vec<SomeStruct> crashed.
//
// A tagged slot carries the type with the value: eight bytes of payload and eight of tag, the
// same shape the compiler already gives a generic struct field. The slot owns a reference to
// what it holds -- set_any takes over the one the push handed it, get_any takes a fresh one for
// the stack, and clear_any gives the slot's back -- so a string or a struct in a container stays
// alive exactly as long as the container does.
//
// A zeroed slot reads as the integer 0, which is what a freshly allocated (and zeroed) region
// has to look like: it owns nothing, so releasing it does nothing.

#define MEM_ANY_TAG_OFFSET 8

static void mem_release_slot(char* slot) {
	int64_t tag;
	memcpy(&tag, slot + MEM_ANY_TAG_OFFSET, sizeof(tag));
	void* held;
	memcpy(&held, slot, sizeof(held));
	if (held == NULL) {
		return;
	}
	if (tag == (int64_t)QD_STACK_TYPE_STR) {
		qd_string_release((qd_string_t*)held);
	} else if (tag == (int64_t)QD_STACK_TYPE_PTR) {
		qd_ptr_release(held);
	}
}

/* Store a value of any type, with its type, in the slot at address+offset */
int usr_mem_set_any(qd_context* ctx) {
	int64_t offset;
	void* address;
	qd_stack_element_t value_elem;

	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}
	if (qd_stack_pop(ctx->st, &value_elem) != QD_STACK_OK) {
		return (int){-1};
	}

	if (address == NULL) {
		MEM_SET_ERR(ctx, MEM_ERR_INVALID_ARG, "mem::set_any: null pointer");
		return (int){-1};
	}

	char* slot = (char*)address + offset;
	mem_release_slot(slot);

	int64_t tag = (int64_t)value_elem.type;
	memcpy(slot, &value_elem.value, sizeof(value_elem.value));
	memcpy(slot + MEM_ANY_TAG_OFFSET, &tag, sizeof(tag));
	return (int){0};
}

/* Read back a value stored by set_any, with the type it was stored as */
int usr_mem_get_any(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}
	if (address == NULL) {
		MEM_SET_ERR(ctx, MEM_ERR_INVALID_ARG, "mem::get_any: null pointer");
		return (int){-1};
	}

	const char* slot = (const char*)address + offset;
	int64_t tag;
	memcpy(&tag, slot + MEM_ANY_TAG_OFFSET, sizeof(tag));

	if (tag == (int64_t)QD_STACK_TYPE_FLOAT) {
		double value;
		memcpy(&value, slot, sizeof(value));
		return qd_push_f(ctx, value);
	}
	if (tag == (int64_t)QD_STACK_TYPE_STR) {
		qd_string_t* value;
		memcpy(&value, slot, sizeof(value));
		if (value == NULL) {
			return qd_push_s(ctx, "");
		}
		return qd_push_s_ref(ctx, value); // retains
	}
	if (tag == (int64_t)QD_STACK_TYPE_PTR) {
		void* value;
		memcpy(&value, slot, sizeof(value));
		qd_ptr_retain(value);
		return qd_push_p(ctx, value);
	}

	int64_t value;
	memcpy(&value, slot, sizeof(value));
	return qd_push_i(ctx, value);
}

/* Release whatever the slot holds and zero it */
int usr_mem_clear_any(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}
	if (address == NULL) {
		MEM_SET_ERR(ctx, MEM_ERR_INVALID_ARG, "mem::clear_any: null pointer");
		return (int){-1};
	}

	char* slot = (char*)address + offset;
	mem_release_slot(slot);
	memset(slot, 0, 16);
	return (int){0};
}
