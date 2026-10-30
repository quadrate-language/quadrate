#include <quadrate/mem/mem.h>
#include <quadrate/rt/runtime.h>
#include <stddef.h>

// Memory operations that don't allocate. Single source, no #ifdef:
// the primitives used here (memcpy, memset, qd_args, qd_fatal_raise)
// are provided by both the hosted runtime (libc-backed) and the
// freestanding runtime (byte-loop memcpy/memset, halting raise). Heap
// functions live in mem_heap.c so they can be physically excluded from
// the freestanding link.
//
// We forward-declare memcpy/memset rather than #include <string.h>
// because that header isn't available in cross-compile freestanding
// configurations (e.g. building for i686-elf without the i386 multilib
// installed). The declarations match libc's exactly.
extern void* memcpy(void* dst, const void* src, size_t n);
extern void* memset(void* dst, int c, size_t n);

// A null address raises. It used to set the error code to -1 and return without pushing,
// so `null 0 mem::get_i64` left the stack a value short and a stray error behind it.
static char* at(qd_context* ctx, const char* op, void* address, int64_t offset) {
	if (address == NULL) {
		qd_fatal_raise(ctx, op, "Null pointer");
	}
	return (char*)address + offset;
}

static size_t size_arg(qd_context* ctx, const char* op, int64_t bytes) {
	if (bytes < 0) {
		qd_fatal_raise(ctx, op, "Negative size");
	}
	return (size_t)bytes;
}

// The integer stores: ( value:i64 address:ptr offset:i64 -- ), writing the low `width` bytes
static int store_int(qd_context* ctx, const char* op, size_t width) {
	int64_t value;
	void* address;
	int64_t offset;
	qd_args(ctx, op, "ipi", &value, &address, &offset);
	char* dst = at(ctx, op, address, offset);
	switch (width) {
	case 1: {
		const uint8_t v8 = (uint8_t)(value & 0xFF);
		memcpy(dst, &v8, 1);
		break;
	}
	case 2: {
		const uint16_t v16 = (uint16_t)(value & 0xFFFF);
		memcpy(dst, &v16, 2);
		break;
	}
	case 4: {
		const uint32_t v32 = (uint32_t)(value & 0xFFFFFFFFu);
		memcpy(dst, &v32, 4);
		break;
	}
	default:
		memcpy(dst, &value, 8);
		break;
	}
	return 0;
}

// The loads: ( address:ptr offset:i64 -- ), giving the address to read
static const char* load_at(qd_context* ctx, const char* op) {
	void* address;
	int64_t offset;
	qd_args(ctx, op, "pi", &address, &offset);
	return at(ctx, op, address, offset);
}

int usr_mem_set_byte(qd_context* ctx) {
	return store_int(ctx, "mem::set_byte", 1);
}

int usr_mem_set_i8(qd_context* ctx) {
	return store_int(ctx, "mem::set_i8", 1);
}

int usr_mem_set_u8(qd_context* ctx) {
	return store_int(ctx, "mem::set_u8", 1);
}

int usr_mem_set_i16(qd_context* ctx) {
	return store_int(ctx, "mem::set_i16", 2);
}

int usr_mem_set_u16(qd_context* ctx) {
	return store_int(ctx, "mem::set_u16", 2);
}

int usr_mem_set_i32(qd_context* ctx) {
	return store_int(ctx, "mem::set_i32", 4);
}

int usr_mem_set_u32(qd_context* ctx) {
	return store_int(ctx, "mem::set_u32", 4);
}

int usr_mem_set_i64(qd_context* ctx) {
	return store_int(ctx, "mem::set_i64", 8);
}

// Loads extend to i64: the signed ones by sign, the unsigned ones by zeros
int usr_mem_get_byte(qd_context* ctx) {
	uint8_t v;
	memcpy(&v, load_at(ctx, "mem::get_byte"), 1);
	return qd_push_i(ctx, (int64_t)v);
}

int usr_mem_get_u8(qd_context* ctx) {
	uint8_t v;
	memcpy(&v, load_at(ctx, "mem::get_u8"), 1);
	return qd_push_i(ctx, (int64_t)v);
}

int usr_mem_get_i8(qd_context* ctx) {
	int8_t v;
	memcpy(&v, load_at(ctx, "mem::get_i8"), 1);
	return qd_push_i(ctx, (int64_t)v);
}

int usr_mem_get_i16(qd_context* ctx) {
	int16_t v;
	memcpy(&v, load_at(ctx, "mem::get_i16"), 2);
	return qd_push_i(ctx, (int64_t)v);
}

int usr_mem_get_u16(qd_context* ctx) {
	uint16_t v;
	memcpy(&v, load_at(ctx, "mem::get_u16"), 2);
	return qd_push_i(ctx, (int64_t)v);
}

int usr_mem_get_i32(qd_context* ctx) {
	int32_t v;
	memcpy(&v, load_at(ctx, "mem::get_i32"), 4);
	return qd_push_i(ctx, (int64_t)v);
}

int usr_mem_get_u32(qd_context* ctx) {
	uint32_t v;
	memcpy(&v, load_at(ctx, "mem::get_u32"), 4);
	return qd_push_i(ctx, (int64_t)v);
}

int usr_mem_get_i64(qd_context* ctx) {
	int64_t v;
	memcpy(&v, load_at(ctx, "mem::get_i64"), 8);
	return qd_push_i(ctx, v);
}

// Stack signature: ( value:f64 address:ptr offset:i64 -- )
int usr_mem_set_f64(qd_context* ctx) {
	double value;
	void* address;
	int64_t offset;
	qd_args(ctx, "mem::set_f64", "fpi", &value, &address, &offset);
	memcpy(at(ctx, "mem::set_f64", address, offset), &value, sizeof(double));
	return 0;
}

int usr_mem_get_f64(qd_context* ctx) {
	double v;
	memcpy(&v, load_at(ctx, "mem::get_f64"), sizeof(double));
	return qd_push_f(ctx, v);
}

// Stack signature: ( value:ptr address:ptr offset:i64 -- )
int usr_mem_set_ptr(qd_context* ctx) {
	void* value;
	void* address;
	int64_t offset;
	qd_args(ctx, "mem::set_ptr", "ppi", &value, &address, &offset);
	memcpy(at(ctx, "mem::set_ptr", address, offset), &value, sizeof(void*));
	return 0;
}

int usr_mem_get_ptr(qd_context* ctx) {
	void* v;
	memcpy(&v, load_at(ctx, "mem::get_ptr"), sizeof(void*));
	return qd_push_p(ctx, v);
}

// Stack signature: ( dst:ptr src:ptr bytes:i64 -- )
int usr_mem_copy(qd_context* ctx) {
	void* dst;
	void* src;
	int64_t bytes;
	qd_args(ctx, "mem::copy", "ppi", &dst, &src, &bytes);
	const size_t n = size_arg(ctx, "mem::copy", bytes);
	memcpy(at(ctx, "mem::copy", dst, 0), at(ctx, "mem::copy", src, 0), n);
	return 0;
}

// Stack signature: ( base:ptr offset:i64 -- result:ptr )
int usr_mem_ptr_add(qd_context* ctx) {
	void* base;
	int64_t offset;
	qd_args(ctx, "mem::ptr_add", "pi", &base, &offset);
	return qd_push_p(ctx, (char*)base + offset);
}

// Stack signature: ( address:ptr bytes:i64 -- )
int usr_mem_zero(qd_context* ctx) {
	void* address;
	int64_t bytes;
	qd_args(ctx, "mem::zero", "pi", &address, &bytes);
	const size_t n = size_arg(ctx, "mem::zero", bytes);
	memset(at(ctx, "mem::zero", address, 0), 0, n);
	return 0;
}

// Stack signature: ( value:i64 address:ptr bytes:i64 -- )
int usr_mem_fill(qd_context* ctx) {
	int64_t value;
	void* address;
	int64_t bytes;
	qd_args(ctx, "mem::fill", "ipi", &value, &address, &bytes);
	const size_t n = size_arg(ctx, "mem::fill", bytes);
	memset(at(ctx, "mem::fill", address, 0), (int)(value & 0xFF), n);
	return 0;
}
