#include <quadrate/mem/mem.h>
#include <quadrate/rt/runtime.h>
#include <quadrate/rt/stack.h>
#include <stddef.h>

// Memory operations that don't allocate. Single source, no #ifdef:
// the libc primitives we use here (memcpy, memset, qd_set_error_msg)
// are provided by both the hosted runtime (libc-backed) and the
// freestanding runtime (byte-loop memcpy/memset, pointer-store
// qd_set_error_msg). Heap functions live in mem_heap.c so they can
// be physically excluded from the freestanding link.
//
// We forward-declare memcpy/memset rather than #include <string.h>
// because that header isn't available in cross-compile freestanding
// configurations (e.g. building for i686-elf without the i386 multilib
// installed). The declarations match libc's exactly.
extern void* memcpy(void* dst, const void* src, size_t n);
extern void* memset(void* dst, int c, size_t n);

// Error codes matching module.qd
#define MEM_ERR_OK 1         // Success (matches builtin Ok)
#define MEM_ERR_ALLOC 2      // Allocation failed
#define MEM_ERR_INVALID_ARG 3  // Invalid argument

#define MEM_SET_ERR(ctx, msg) do {                                                                                    \
	(ctx)->error_code = -1;                                                                                           \
	qd_set_error_msg(ctx, msg);                                                                                       \
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
		MEM_SET_ERR(ctx, "Null pointer in mem::set_byte");
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
		MEM_SET_ERR(ctx, "Null pointer in mem::get_byte");
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
		MEM_SET_ERR(ctx, "Null pointer in mem::set_i64");
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
		MEM_SET_ERR(ctx, "Null pointer in mem::get_i64");
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
		MEM_SET_ERR(ctx, "Null pointer in mem::set_f64");
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
		MEM_SET_ERR(ctx, "Null pointer in mem::get_f64");
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
		MEM_SET_ERR(ctx, "Null pointer in mem::set_ptr");
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
		MEM_SET_ERR(ctx, "Null pointer in mem::get_ptr");
		return (int){-1};
	}

	void* value;
	memcpy(&value, (char*)address + offset, sizeof(void*));
	return qd_push_p(ctx, value);
}

/* Sized integer accessors. Stack values are always i64; these narrow on
 * store and extend (sign or zero) on load, so user code can use i64
 * arithmetic freely and only worry about widths at memory boundaries. */

/* set_i8 / set_u8 share one implementation: the low byte is written. */
static int store_narrow_int(qd_context* ctx, const char* label, size_t width) {
	int64_t value, offset;
	void* address;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &address) != QD_STACK_OK ||
			pop_int(ctx, &value) != QD_STACK_OK) {
		return -1;
	}

	if (address == NULL) {
		(void)label;
		MEM_SET_ERR(ctx, "Null pointer in mem sized-int store");
		return -1;
	}

	char* dst = (char*)address + offset;
	switch (width) {
	case 1: *(uint8_t*)dst = (uint8_t)(value & 0xFF); break;
	case 2: {
		uint16_t v16 = (uint16_t)(value & 0xFFFF);
		memcpy(dst, &v16, 2);
		break;
	}
	case 4: {
		uint32_t v32 = (uint32_t)(value & 0xFFFFFFFFu);
		memcpy(dst, &v32, 4);
		break;
	}
	}
	return 0;
}

int usr_mem_set_i8(qd_context* ctx) { return store_narrow_int(ctx, "set_i8", 1); }
int usr_mem_set_u8(qd_context* ctx) { return store_narrow_int(ctx, "set_u8", 1); }
int usr_mem_set_i16(qd_context* ctx) { return store_narrow_int(ctx, "set_i16", 2); }
int usr_mem_set_u16(qd_context* ctx) { return store_narrow_int(ctx, "set_u16", 2); }
int usr_mem_set_i32(qd_context* ctx) { return store_narrow_int(ctx, "set_i32", 4); }
int usr_mem_set_u32(qd_context* ctx) { return store_narrow_int(ctx, "set_u32", 4); }

int usr_mem_get_i8(qd_context* ctx) {
	int64_t offset; void* address;
	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) return -1;
	if (address == NULL) { MEM_SET_ERR(ctx, "Null pointer in mem::get_i8"); return -1; }
	int8_t v = *((int8_t*)((char*)address + offset));
	return qd_push_i(ctx, (int64_t)v);  /* sign-extend */
}
int usr_mem_get_u8(qd_context* ctx) {
	int64_t offset; void* address;
	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) return -1;
	if (address == NULL) { MEM_SET_ERR(ctx, "Null pointer in mem::get_u8"); return -1; }
	uint8_t v = *((uint8_t*)((char*)address + offset));
	return qd_push_i(ctx, (int64_t)v);  /* zero-extend */
}
int usr_mem_get_i16(qd_context* ctx) {
	int64_t offset; void* address;
	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) return -1;
	if (address == NULL) { MEM_SET_ERR(ctx, "Null pointer in mem::get_i16"); return -1; }
	int16_t v; memcpy(&v, (char*)address + offset, 2);
	return qd_push_i(ctx, (int64_t)v);
}
int usr_mem_get_u16(qd_context* ctx) {
	int64_t offset; void* address;
	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) return -1;
	if (address == NULL) { MEM_SET_ERR(ctx, "Null pointer in mem::get_u16"); return -1; }
	uint16_t v; memcpy(&v, (char*)address + offset, 2);
	return qd_push_i(ctx, (int64_t)v);
}
int usr_mem_get_i32(qd_context* ctx) {
	int64_t offset; void* address;
	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) return -1;
	if (address == NULL) { MEM_SET_ERR(ctx, "Null pointer in mem::get_i32"); return -1; }
	int32_t v; memcpy(&v, (char*)address + offset, 4);
	return qd_push_i(ctx, (int64_t)v);
}
int usr_mem_get_u32(qd_context* ctx) {
	int64_t offset; void* address;
	if (pop_int(ctx, &offset) != QD_STACK_OK || pop_ptr(ctx, &address) != QD_STACK_OK) return -1;
	if (address == NULL) { MEM_SET_ERR(ctx, "Null pointer in mem::get_u32"); return -1; }
	uint32_t v; memcpy(&v, (char*)address + offset, 4);
	return qd_push_i(ctx, (int64_t)v);
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
		MEM_SET_ERR(ctx, "Null pointer in mem::copy");
		return (int){-1};
	}

	if (bytes < 0) {
		MEM_SET_ERR(ctx, "Negative size in mem::copy");
		return (int){-1};
	}

	memcpy(dst, src, (size_t)bytes);
	return (int){0};
}

/* Pointer arithmetic: base + offset -> result */
int usr_mem_ptr_add(qd_context* ctx) {
	int64_t offset;
	void* base;

	if (pop_int(ctx, &offset) != QD_STACK_OK ||
			pop_ptr(ctx, &base) != QD_STACK_OK) {
		return (int){-1};
	}

	void* result = (char*)base + offset;
	qd_push_p(ctx, result);
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
		MEM_SET_ERR(ctx, "Null pointer in mem::zero");
		return (int){-1};
	}

	if (bytes < 0) {
		MEM_SET_ERR(ctx, "Negative size in mem::zero");
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
		MEM_SET_ERR(ctx, "Null pointer in mem::fill");
		return (int){-1};
	}

	if (bytes < 0) {
		MEM_SET_ERR(ctx, "Negative size in mem::fill");
		return (int){-1};
	}

	memset(address, (int)(value & 0xFF), (size_t)bytes);
	return (int){0};
}

