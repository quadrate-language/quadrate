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

#define MEM_SET_ERR(ctx, msg) do {                                                                                    \
	(ctx)->error_code = -1;                                                                                           \
	qd_set_error_msg(ctx, msg);                                                                                       \
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
		qd_push_i(ctx, MEM_ERR_INVALID_ARG);
		return (int){MEM_ERR_INVALID_ARG};
	}

	if (bytes < 0) {
		qd_push_i(ctx, MEM_ERR_INVALID_ARG);
		return (int){MEM_ERR_INVALID_ARG};
	}

	void* ptr = malloc((size_t)bytes);
	if (ptr == NULL && bytes > 0) {
		qd_push_i(ctx, MEM_ERR_ALLOC);
		return (int){MEM_ERR_ALLOC};
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
		qd_push_i(ctx, MEM_ERR_INVALID_ARG);
		return (int){MEM_ERR_INVALID_ARG};
	}

	if (pop_ptr(ctx, &ptr) != QD_STACK_OK) {
		qd_push_i(ctx, MEM_ERR_INVALID_ARG);
		return (int){MEM_ERR_INVALID_ARG};
	}

	if (new_bytes < 0) {
		qd_push_i(ctx, MEM_ERR_INVALID_ARG);
		return (int){MEM_ERR_INVALID_ARG};
	}

	void* new_ptr = realloc(ptr, (size_t)new_bytes);
	if (new_ptr == NULL && new_bytes > 0) {
		qd_push_i(ctx, MEM_ERR_ALLOC);
		return (int){MEM_ERR_ALLOC};
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
		qd_push_i(ctx, MEM_ERR_INVALID_ARG);
		return (int){MEM_ERR_INVALID_ARG};
	}

	if (pop_int(ctx, &alignment) != QD_STACK_OK) {
		qd_push_i(ctx, MEM_ERR_INVALID_ARG);
		return (int){MEM_ERR_INVALID_ARG};
	}

	if (bytes < 0 || alignment < 1) {
		qd_push_i(ctx, MEM_ERR_INVALID_ARG);
		return (int){MEM_ERR_INVALID_ARG};
	}

	if ((alignment & (alignment - 1)) != 0) {
		qd_push_i(ctx, MEM_ERR_INVALID_ARG);
		return (int){MEM_ERR_INVALID_ARG};
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
		qd_push_i(ctx, MEM_ERR_ALLOC);
		return (int){MEM_ERR_ALLOC};
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
		MEM_SET_ERR(ctx, "Null pointer in mem::to_string");
		return (int){-1};
	}

	if (length < 0) {
		MEM_SET_ERR(ctx, "Negative length in mem::to_string");
		return (int){-1};
	}

	char* str = malloc((size_t)length + 1);
	if (!str) {
		MEM_SET_ERR(ctx, "Allocation failed in mem::to_string");
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
		MEM_SET_ERR(ctx, "Allocation failed in mem::from_string");
		return (int){-1};
	}

	memcpy(buffer, str, length);
	qd_string_release(str_elem.value.s);

	qd_push_p(ctx, buffer);
	return qd_push_i(ctx, (int64_t)length);
}
