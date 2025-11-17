#include <stdmemqd/mem.h>
#include <qdrt/stack.h>
#include <stdlib.h>
#include <string.h>

// These functions provide memory management for Quadrate programs
// Named with usr_ prefix for the import mechanism
// Implementations are duplicated from qdrt/src/memory.c to avoid link-order issues

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
qd_exec_result usr_mem_alloc(qd_context* ctx) {
	int64_t bytes;
	if (pop_int(ctx, &bytes) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (bytes < 0) {
		return qd_push_p(ctx, NULL);
	}

	void* ptr = malloc((size_t)bytes);
	return qd_push_p(ctx, ptr);
}

/* Free memory */
qd_exec_result usr_mem_free(qd_context* ctx) {
	void* ptr;
	if (pop_ptr(ctx, &ptr) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	free(ptr);
	return (qd_exec_result){0};
}

/* Reallocate memory */
qd_exec_result usr_mem_realloc(qd_context* ctx) {
	int64_t new_bytes;
	void* ptr;

	if (pop_int(ctx, &new_bytes) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (pop_ptr(ctx, &ptr) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (new_bytes < 0) {
		return qd_push_p(ctx, NULL);
	}

	void* new_ptr = realloc(ptr, (size_t)new_bytes);
	return qd_push_p(ctx, new_ptr);
}

/* Set byte at address */
qd_exec_result usr_mem_set_byte(qd_context* ctx) {
	int64_t value, offset;
	void* address;

	if (pop_int(ctx, &value) != QD_STACK_OK ||
			pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in mem::set_byte";
		return (qd_exec_result){-1};
	}

	*((uint8_t*)((char*)address + offset)) = (uint8_t)(value & 0xFF);
	return (qd_exec_result){0};
}

/* Get byte from address */
qd_exec_result usr_mem_get_byte(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in mem::get_byte";
		return (qd_exec_result){-1};
	}

	uint8_t byte = *((uint8_t*)((char*)address + offset));
	return qd_push_i(ctx, (int64_t)byte);
}

/* Set 64-bit integer at address */
qd_exec_result usr_mem_set(qd_context* ctx) {
	int64_t value, offset;
	void* address;

	if (pop_int(ctx, &value) != QD_STACK_OK ||
			pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in mem::set";
		return (qd_exec_result){-1};
	}

	memcpy((char*)address + offset, &value, sizeof(int64_t));
	return (qd_exec_result){0};
}

/* Get 64-bit integer from address */
qd_exec_result usr_mem_get(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in mem::get";
		return (qd_exec_result){-1};
	}

	int64_t value;
	memcpy(&value, (char*)address + offset, sizeof(int64_t));
	return qd_push_i(ctx, value);
}

/* Set float at address */
qd_exec_result usr_mem_set_float(qd_context* ctx) {
	double value;
	int64_t offset;
	void* address;

	if (pop_float(ctx, &value) != QD_STACK_OK ||
			pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in mem::set_float";
		return (qd_exec_result){-1};
	}

	memcpy((char*)address + offset, &value, sizeof(double));
	return (qd_exec_result){0};
}

/* Get float from address */
qd_exec_result usr_mem_get_float(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in mem::get_float";
		return (qd_exec_result){-1};
	}

	double value;
	memcpy(&value, (char*)address + offset, sizeof(double));
	return qd_push_f(ctx, value);
}

/* Set pointer at address */
qd_exec_result usr_mem_set_ptr(qd_context* ctx) {
	void *value, *address;
	int64_t offset;

	if (pop_ptr(ctx, &value) != QD_STACK_OK ||
			pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in mem::set_ptr";
		return (qd_exec_result){-1};
	}

	memcpy((char*)address + offset, &value, sizeof(void*));
	return (qd_exec_result){0};
}

/* Get pointer from address */
qd_exec_result usr_mem_get_ptr(qd_context* ctx) {
	int64_t offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in mem::get_ptr";
		return (qd_exec_result){-1};
	}

	void* value;
	memcpy(&value, (char*)address + offset, sizeof(void*));
	return qd_push_p(ctx, value);
}

/* Copy memory */
qd_exec_result usr_mem_copy(qd_context* ctx) {
	int64_t bytes;
	void *dst, *src;

	if (pop_int(ctx, &bytes) != QD_STACK_OK ||
			pop_ptr(ctx, &dst) != QD_STACK_OK ||
			pop_ptr(ctx, &src) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (src == NULL || dst == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in mem::copy";
		return (qd_exec_result){-1};
	}

	if (bytes < 0) {
		ctx->error_code = -1;
		ctx->error_msg = "Negative size in mem::copy";
		return (qd_exec_result){-1};
	}

	memcpy(dst, src, (size_t)bytes);
	return (qd_exec_result){0};
}

/* Zero memory */
qd_exec_result usr_mem_zero(qd_context* ctx) {
	int64_t bytes;
	void* address;

	if (pop_int(ctx, &bytes) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in mem::zero";
		return (qd_exec_result){-1};
	}

	if (bytes < 0) {
		ctx->error_code = -1;
		ctx->error_msg = "Negative size in mem::zero";
		return (qd_exec_result){-1};
	}

	memset(address, 0, (size_t)bytes);
	return (qd_exec_result){0};
}

/* Fill memory with byte value */
qd_exec_result usr_mem_fill(qd_context* ctx) {
	int64_t value, bytes;
	void* address;

	if (pop_int(ctx, &value) != QD_STACK_OK ||
			pop_int(ctx, &bytes) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (address == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in mem::fill";
		return (qd_exec_result){-1};
	}

	if (bytes < 0) {
		ctx->error_code = -1;
		ctx->error_msg = "Negative size in mem::fill";
		return (qd_exec_result){-1};
	}

	memset(address, (int)(value & 0xFF), (size_t)bytes);
	return (qd_exec_result){0};
}

/* Convert buffer to string */
qd_exec_result usr_mem_to_string(qd_context* ctx) {
	int64_t length;
	void* buffer;

	if (pop_int(ctx, &length) != QD_STACK_OK ||
			pop_ptr(ctx, &buffer) != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}

	if (buffer == NULL) {
		ctx->error_code = -1;
		ctx->error_msg = "Null pointer in mem::to_string";
		return (qd_exec_result){-1};
	}

	if (length < 0) {
		ctx->error_code = -1;
		ctx->error_msg = "Negative length in mem::to_string";
		return (qd_exec_result){-1};
	}

	// Allocate string with null terminator
	char* str = malloc((size_t)length + 1);
	if (!str) {
		ctx->error_code = -1;
		ctx->error_msg = "Allocation failed in mem::to_string";
		return (qd_exec_result){-1};
	}

	// Copy buffer to string
	memcpy(str, buffer, (size_t)length);
	str[length] = '\0';

	qd_exec_result result = qd_push_s(ctx, str);

	// qd_push_s makes a copy, so free the original
	free(str);

	return result;
}

/* Convert string to buffer */
qd_exec_result usr_mem_from_string(qd_context* ctx) {
	qd_stack_element_t str_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &str_elem);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-1};
	}
	if (str_elem.type != QD_STACK_TYPE_STR) {
		free(str_elem.value.s);
		return (qd_exec_result){-1};
	}

	char* str = str_elem.value.s;
	size_t length = strlen(str);

	// Allocate buffer
	void* buffer = malloc(length);
	if (!buffer) {
		free(str);
		ctx->error_code = -1;
		ctx->error_msg = "Allocation failed in mem::from_string";
		return (qd_exec_result){-1};
	}

	// Copy string to buffer (without null terminator)
	memcpy(buffer, str, length);
	free(str);

	// Push buffer and length
	qd_push_p(ctx, buffer);
	return qd_push_i(ctx, (int64_t)length);
}
