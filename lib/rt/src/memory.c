#define _POSIX_C_SOURCE 200809L
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <stdlib.h>
#include <string.h>

/* Macro for null pointer error handling */
#define CHECK_NULL(ctx, ptr, op_name)                                                                                  \
	do {                                                                                                               \
		if ((ptr) == NULL) {                                                                                           \
			(ctx)->error_code = -1;                                                                                    \
			qd_set_error_msg((ctx), "Null pointer in mem::" op_name);                                                  \
			return (int){-1};                                                                                          \
		}                                                                                                              \
	} while (0)

/* Helper: Pop integer from stack */
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

/* Helper: Pop float from stack */
static qd_stack_error pop_float(qd_context* ctx, double* value) {
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		return err;
	}
	if (elem.type != QD_STACK_TYPE_FLOAT) {
		return QD_STACK_ERR_TYPE_MISMATCH;
	}
	*value = elem.value.f;
	return QD_STACK_OK;
}

/* Helper: Pop pointer from stack */
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
int qd_mem_alloc(qd_context* ctx) {
	int64_t bytes;
	if (pop_int(ctx, &bytes) != QD_STACK_OK) {
		return (int){-1};
	}

	if (bytes < 0) {
		ctx->error_code = -1;
		qd_set_error_msg(ctx, "Negative size in mem::alloc");
		return (int){-1};
	}

	void* ptr = malloc((size_t)bytes);
	if (ptr == NULL && bytes > 0) {
		ctx->error_code = -1;
		qd_set_error_msg(ctx, "Allocation failed in mem::alloc");
		return (int){-1};
	}
	return qd_push_p(ctx, ptr);
}

/* Free memory */
int qd_mem_free(qd_context* ctx) {
	void* ptr;
	if (pop_ptr(ctx, &ptr) != QD_STACK_OK) {
		return (int){-1};
	}

	free(ptr);
	return (int){0};
}

/* Reallocate memory */
int qd_mem_realloc(qd_context* ctx) {
	int64_t new_bytes;
	void* ptr;

	if (pop_int(ctx, &new_bytes) != QD_STACK_OK) {
		return (int){-1};
	}

	if (pop_ptr(ctx, &ptr) != QD_STACK_OK) {
		return (int){-1};
	}

	if (new_bytes < 0) {
		ctx->error_code = -1;
		qd_set_error_msg(ctx, "Negative size in mem::realloc");
		return (int){-1};
	}

	void* new_ptr = realloc(ptr, (size_t)new_bytes);
	if (new_ptr == NULL && new_bytes > 0) {
		/* Reallocation failed. The original block is still valid, so push it
		 * back to avoid leaking it, but report the failure so the caller does
		 * not assume it now owns a larger buffer. */
		ctx->error_code = -1;
		qd_set_error_msg(ctx, "Allocation failed in mem::realloc");
		qd_push_p(ctx, ptr);
		return (int){-1};
	}
	return qd_push_p(ctx, new_ptr);
}

/* Set byte at address */
int qd_mem_set_byte(qd_context* ctx) {
	int64_t value, offset;
	void* address;

	if (pop_int(ctx, &value) != QD_STACK_OK || pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	CHECK_NULL(ctx, address, "set_byte");

	*((uint8_t*)((char*)address + offset)) = (uint8_t)(value & 0xFF);
	return (int){0};
}

/* Get byte from address */
int qd_mem_get_byte(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	CHECK_NULL(ctx, address, "get_byte");

	uint8_t byte = *((uint8_t*)((char*)address + offset));
	return qd_push_i(ctx, (int64_t)byte);
}

/* Set 64-bit integer at address */
int qd_mem_set(qd_context* ctx) {
	int64_t value, offset;
	void* address;

	if (pop_int(ctx, &value) != QD_STACK_OK || pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	CHECK_NULL(ctx, address, "set");

	memcpy((char*)address + offset, &value, sizeof(int64_t));
	return (int){0};
}

/* Get 64-bit integer from address */
int qd_mem_get(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	CHECK_NULL(ctx, address, "get");

	int64_t value;
	memcpy(&value, (char*)address + offset, sizeof(int64_t));
	return qd_push_i(ctx, value);
}

/* Set float at address */
int qd_mem_set_float(qd_context* ctx) {
	double value;
	int64_t offset;
	void* address;

	if (pop_float(ctx, &value) != QD_STACK_OK || pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	CHECK_NULL(ctx, address, "set_float");

	memcpy((char*)address + offset, &value, sizeof(double));
	return (int){0};
}

/* Get float from address */
int qd_mem_get_float(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	CHECK_NULL(ctx, address, "get_float");

	double value;
	memcpy(&value, (char*)address + offset, sizeof(double));
	return qd_push_f(ctx, value);
}

/* Set pointer at address */
int qd_mem_set_ptr(qd_context* ctx) {
	void *value, *address;
	int64_t offset;

	if (pop_ptr(ctx, &value) != QD_STACK_OK || pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	CHECK_NULL(ctx, address, "set_ptr");

	memcpy((char*)address + offset, &value, sizeof(void*));
	return (int){0};
}

/* Get pointer from address */
int qd_mem_get_ptr(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	CHECK_NULL(ctx, address, "get_ptr");

	void* value;
	memcpy(&value, (char*)address + offset, sizeof(void*));
	return qd_push_p(ctx, value);
}

/* Copy memory */
int qd_mem_copy(qd_context* ctx) {
	int64_t bytes;
	void *dst, *src;

	if (pop_int(ctx, &bytes) != QD_STACK_OK || pop_ptr(ctx, &dst) != QD_STACK_OK || pop_ptr(ctx, &src) != QD_STACK_OK) {
		return (int){-1};
	}

	CHECK_NULL(ctx, src, "copy");
	CHECK_NULL(ctx, dst, "copy");

	if (bytes < 0) {
		ctx->error_code = -1;
		qd_set_error_msg(ctx, "Negative size in mem::copy");
		return (int){-1};
	}

	memcpy(dst, src, (size_t)bytes);
	return (int){0};
}

/* Zero memory */
int qd_mem_zero(qd_context* ctx) {
	int64_t bytes;
	void* address;

	if (pop_int(ctx, &bytes) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	CHECK_NULL(ctx, address, "zero");

	if (bytes < 0) {
		ctx->error_code = -1;
		qd_set_error_msg(ctx, "Negative size in mem::zero");
		return (int){-1};
	}

	memset(address, 0, (size_t)bytes);
	return (int){0};
}

/* Fill memory with byte value */
int qd_mem_fill(qd_context* ctx) {
	int64_t value, bytes;
	void* address;

	if (pop_int(ctx, &value) != QD_STACK_OK || pop_int(ctx, &bytes) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	CHECK_NULL(ctx, address, "fill");

	if (bytes < 0) {
		ctx->error_code = -1;
		qd_set_error_msg(ctx, "Negative size in mem::fill");
		return (int){-1};
	}

	memset(address, (int)(value & 0xFF), (size_t)bytes);
	return (int){0};
}
