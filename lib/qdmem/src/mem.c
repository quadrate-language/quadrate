#define _POSIX_C_SOURCE 200809L
#include <qdmem/mem.h>
#include <qdrt/stack.h>
#include <stdlib.h>
#include <string.h>

// These functions provide memory management for Quadrate programs
// Named with usr_ prefix for the import mechanism
// Implementations are duplicated from qdrt/src/memory.c to avoid link-order issues

// Error codes matching module.qd
#define MEM_ERR_OK 1         // Success (matches builtin Ok)
#define MEM_ERR_ALLOC 2      // Allocation failed
#define MEM_ERR_INVALID_ARG 3  // Invalid argument

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

	// Push result then Ok
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

	// Push result then Ok
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

	// Alignment must be a power of 2
	if ((alignment & (alignment - 1)) != 0) {
		qd_push_i(ctx, MEM_ERR_INVALID_ARG);
		return (int){MEM_ERR_INVALID_ARG};
	}

	// Use aligned_alloc (C11) - alignment must also be a multiple of sizeof(void*)
	// and size must be a multiple of alignment
	size_t align = (size_t)alignment;
	if (align < sizeof(void*)) {
		align = sizeof(void*);
	}
	size_t size = (size_t)bytes;
	// Round size up to multiple of alignment
	if (size % align != 0) {
		size = ((size / align) + 1) * align;
	}

	void* ptr = aligned_alloc(align, size);
	if (ptr == NULL && bytes > 0) {
		qd_push_i(ctx, MEM_ERR_ALLOC);
		return (int){MEM_ERR_ALLOC};
	}

	// Push result then Ok
	qd_push_p(ctx, ptr);
	qd_push_i(ctx, MEM_ERR_OK);
	return (int){0};
}

/* Set byte at address */
int usr_mem_set_byte(qd_context* ctx) {
	int64_t value, offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK ||
			pop_int(ctx, &value) != QD_STACK_OK) {
		return (int){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Null pointer in mem::set_byte");
		return (int){-1};
	}

	*((uint8_t*)((char*)address + offset)) = (uint8_t)(value & 0xFF);
	return (int){0};
}

/* Get byte from address */
int usr_mem_get_byte(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Null pointer in mem::get_byte");
		return (int){-1};
	}

	uint8_t byte = *((uint8_t*)((char*)address + offset));
	return qd_push_i(ctx, (int64_t)byte);
}

/* Set 64-bit integer at address */
int usr_mem_set_i64(qd_context* ctx) {
	int64_t value, offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK ||
			pop_int(ctx, &value) != QD_STACK_OK) {
		return (int){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Null pointer in mem::set_i64");
		return (int){-1};
	}

	memcpy((char*)address + offset, &value, sizeof(int64_t));
	return (int){0};
}

/* Get 64-bit integer from address */
int usr_mem_get_i64(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Null pointer in mem::get_i64");
		return (int){-1};
	}

	int64_t value;
	memcpy(&value, (char*)address + offset, sizeof(int64_t));
	return qd_push_i(ctx, value);
}

/* Set 64-bit float at address */
int usr_mem_set_f64(qd_context* ctx) {
	double value;
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK ||
			pop_float(ctx, &value) != QD_STACK_OK) {
		return (int){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Null pointer in mem::set_f64");
		return (int){-1};
	}

	memcpy((char*)address + offset, &value, sizeof(double));
	return (int){0};
}

/* Get 64-bit float from address */
int usr_mem_get_f64(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Null pointer in mem::get_f64");
		return (int){-1};
	}

	double value;
	memcpy(&value, (char*)address + offset, sizeof(double));
	return qd_push_f(ctx, value);
}

/* Set pointer at address */
int usr_mem_set_ptr(qd_context* ctx) {
	void *value, *address;
	int64_t offset;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK ||
			pop_ptr(ctx, &value) != QD_STACK_OK) {
		return (int){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Null pointer in mem::set_ptr");
		return (int){-1};
	}

	memcpy((char*)address + offset, &value, sizeof(void*));
	return (int){0};
}

/* Get pointer from address */
int usr_mem_get_ptr(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Null pointer in mem::get_ptr");
		return (int){-1};
	}

	void* value;
	memcpy(&value, (char*)address + offset, sizeof(void*));
	return qd_push_p(ctx, value);
}

/* Copy memory: dst src bytes -> memcpy(dst, src, bytes) */
int usr_mem_copy(qd_context* ctx) {
	int64_t bytes;
	void *dst, *src;

	if (pop_int(ctx, &bytes) != QD_STACK_OK ||
			pop_ptr(ctx, &src) != QD_STACK_OK ||
			pop_ptr(ctx, &dst) != QD_STACK_OK) {
		return (int){-1};
	}

	if (src == NULL || dst == NULL) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Null pointer in mem::copy");
		return (int){-1};
	}

	if (bytes < 0) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Negative size in mem::copy");
		return (int){-1};
	}

	memcpy(dst, src, (size_t)bytes);
	return (int){0};
}

/* Zero memory */
int usr_mem_zero(qd_context* ctx) {
	int64_t bytes;
	void* address;

	if (pop_int(ctx, &bytes) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (int){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Null pointer in mem::zero");
		return (int){-1};
	}

	if (bytes < 0) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Negative size in mem::zero");
		return (int){-1};
	}

	memset(address, 0, (size_t)bytes);
	return (int){0};
}

/* Fill memory with byte value */
int usr_mem_fill(qd_context* ctx) {
	int64_t value, bytes;
	void* address;

	if (pop_int(ctx, &bytes) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK ||
			pop_int(ctx, &value) != QD_STACK_OK) {
		return (int){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Null pointer in mem::fill");
		return (int){-1};
	}

	if (bytes < 0) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Negative size in mem::fill");
		return (int){-1};
	}

	memset(address, (int)(value & 0xFF), (size_t)bytes);
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
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Null pointer in mem::to_string");
		return (int){-1};
	}

	if (length < 0) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Negative length in mem::to_string");
		return (int){-1};
	}

	// Allocate string with null terminator
	char* str = malloc((size_t)length + 1);
	if (!str) {
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Allocation failed in mem::to_string");
		return (int){-1};
	}

	// Copy buffer to string
	memcpy(str, buffer, (size_t)length);
	str[length] = '\0';

	int result = qd_push_s(ctx, str);

	// qd_push_s makes a copy, so free the original
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

	// Allocate buffer
	void* buffer = malloc(length);
	if (!buffer) {
		qd_string_release(str_elem.value.s);
		ctx->error_code = -1;
		free(ctx->error_msg);
		ctx->error_msg = strdup("Allocation failed in mem::from_string");
		return (int){-1};
	}

	// Copy string to buffer (without null terminator)
	memcpy(buffer, str, length);
	qd_string_release(str_elem.value.s);

	// Push buffer and length
	qd_push_p(ctx, buffer);
	return qd_push_i(ctx, (int64_t)length);
}
