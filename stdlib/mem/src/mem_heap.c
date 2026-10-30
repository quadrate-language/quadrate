// mem_heap.c — heap-using mem functions: alloc, realloc, alloc_aligned,
// free (it's also a heap fn — it's the partner to alloc), to_string,
// from_string. These are physically separate from mem.c so the
// freestanding build can omit them entirely. Linking this file into a
// freestanding binary would drag in malloc/realloc/aligned_alloc/free
// from libc, which is exactly what we want to avoid.

#define _POSIX_C_SOURCE 200809L
#include <quadrate/mem/mem.h>
#include <quadrate/rt/runtime.h>
#include <stdlib.h>
#include <string.h>

#define MEM_ERR_OK 1
#define MEM_ERR_ALLOC 2
#define MEM_ERR_INVALID_ARG 3

static int push_ok(qd_context* ctx, void* address) {
	qd_push_p(ctx, address);
	return qd_push_i(ctx, MEM_ERR_OK);
}

// Stack signature: ( bytes:i64 -- address:ptr )!
int usr_mem_alloc(qd_context* ctx) {
	int64_t bytes;
	qd_args(ctx, "mem::alloc", "i", &bytes);
	if (bytes < 0) {
		return qd_fail(ctx, MEM_ERR_INVALID_ARG, "mem::alloc: negative byte count");
	}
	void* ptr = malloc((size_t)bytes);
	if (ptr == NULL && bytes > 0) {
		return qd_fail(ctx, MEM_ERR_ALLOC, "mem::alloc: out of memory");
	}
	return push_ok(ctx, ptr);
}

// Stack signature: ( address:ptr new_bytes:i64 -- new_address:ptr )!
int usr_mem_realloc(qd_context* ctx) {
	void* ptr;
	int64_t new_bytes;
	qd_args(ctx, "mem::realloc", "pi", &ptr, &new_bytes);
	if (new_bytes < 0) {
		return qd_fail(ctx, MEM_ERR_INVALID_ARG, "mem::realloc: negative byte count");
	}
	void* new_ptr = realloc(ptr, (size_t)new_bytes);
	if (new_ptr == NULL && new_bytes > 0) {
		return qd_fail(ctx, MEM_ERR_ALLOC, "mem::realloc: out of memory");
	}
	return push_ok(ctx, new_ptr);
}

// Stack signature: ( alignment:i64 bytes:i64 -- address:ptr )!
int usr_mem_alloc_aligned(qd_context* ctx) {
	int64_t alignment;
	int64_t bytes;
	qd_args(ctx, "mem::alloc_aligned", "ii", &alignment, &bytes);
	if (bytes < 0 || alignment < 1) {
		return qd_fail(ctx, MEM_ERR_INVALID_ARG, "mem::alloc_aligned: negative size or alignment below 1");
	}
	if ((alignment & (alignment - 1)) != 0) {
		return qd_fail(ctx, MEM_ERR_INVALID_ARG, "mem::alloc_aligned: alignment is not a power of two");
	}
	size_t align = (size_t)alignment;
	if (align < sizeof(void*)) {
		align = sizeof(void*);
	}
	// aligned_alloc wants a size that is a multiple of the alignment
	size_t size = (size_t)bytes;
	if (size % align != 0) {
		size = ((size / align) + 1) * align;
	}
	void* ptr = aligned_alloc(align, size);
	if (ptr == NULL && bytes > 0) {
		return qd_fail(ctx, MEM_ERR_ALLOC, "mem::alloc_aligned: out of memory");
	}
	return push_ok(ctx, ptr);
}

// Stack signature: ( buffer:ptr length:i64 -- text:str )
// All `length` bytes, a NUL among them included
int usr_mem_to_string(qd_context* ctx) {
	void* buffer;
	int64_t length;
	qd_args(ctx, "mem::to_string", "pi", &buffer, &length);
	if (buffer == NULL) {
		qd_fatal_raise(ctx, "mem::to_string", "Null pointer");
	}
	if (length < 0) {
		qd_fatal_raise(ctx, "mem::to_string", "Negative length");
	}
	qd_string_t* text = qd_string_create_with_length((const char*)buffer, (size_t)length);
	if (!text) {
		qd_fatal_raise(ctx, "mem::to_string", "Out of memory");
	}
	qd_push_s_ref(ctx, text);
	qd_string_release(text);
	return 0;
}

// Stack signature: ( text:str -- buffer:ptr length:i64 )
// All of the string, past a NUL byte too; the buffer is the caller's to mem::free
int usr_mem_from_string(qd_context* ctx) {
	qd_string_t* text;
	qd_args(ctx, "mem::from_string", "s", &text);
	const size_t length = qd_string_length(text);
	// One byte at least, so an empty string gets a buffer rather than a null that reads as failure
	void* buffer = malloc(length > 0 ? length : 1);
	if (!buffer) {
		qd_string_release(text);
		qd_fatal_raise(ctx, "mem::from_string", "Out of memory");
	}
	memcpy(buffer, qd_string_data(text), length);
	qd_string_release(text);
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

//
// A d128 is sixteen bytes and does not fit the payload, so the slot holds it boxed: a
// pointer to a copy of its own, freed with the slot. It used to be copied in whole, its
// upper half over the tag, and read back as its coefficient: 1.25d came out as 125.

#define MEM_ANY_TAG_OFFSET 8

static char* slot_at(qd_context* ctx, const char* op, void* address, int64_t offset) {
	if (address == NULL) {
		qd_fatal_raise(ctx, op, "Null pointer");
	}
	return (char*)address + offset;
}

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
	} else if (tag == (int64_t)QD_STACK_TYPE_D128) {
		free(held);
	}
}

// Stack signature: ( value:any address:ptr offset:i64 -- )
int usr_mem_set_any(qd_context* ctx) {
	qd_stack_element_t value;
	void* address;
	int64_t offset;
	qd_args(ctx, "mem::set_any", "vpi", &value, &address, &offset);
	if (address == NULL) {
		if (value.type == QD_STACK_TYPE_STR) {
			qd_string_release(value.value.s);
		} else if (value.type == QD_STACK_TYPE_PTR) {
			qd_ptr_release(value.value.p);
		}
		qd_fatal_raise(ctx, "mem::set_any", "Null pointer");
	}
	char* slot = (char*)address + offset;

	// The eight bytes the slot keeps: the value itself, or for a d128 its box
	uint64_t payload = 0;
	if (value.type == QD_STACK_TYPE_D128) {
		qd_d128_t* box = malloc(sizeof(qd_d128_t));
		if (!box) {
			qd_fatal_raise(ctx, "mem::set_any", "Out of memory");
		}
		*box = value.value.d;
		memcpy(&payload, &box, sizeof(box));
	} else {
		memcpy(&payload, &value.value, sizeof(payload));
	}

	mem_release_slot(slot);
	const int64_t tag = (int64_t)value.type;
	memcpy(slot, &payload, sizeof(payload));
	memcpy(slot + MEM_ANY_TAG_OFFSET, &tag, sizeof(tag));
	return 0;
}

// Stack signature: ( address:ptr offset:i64 -- value:any )
int usr_mem_get_any(qd_context* ctx) {
	void* address;
	int64_t offset;
	qd_args(ctx, "mem::get_any", "pi", &address, &offset);
	const char* slot = slot_at(ctx, "mem::get_any", address, offset);
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
		return value ? qd_push_s_ref(ctx, value) : qd_push_s(ctx, ""); // the push retains
	}
	if (tag == (int64_t)QD_STACK_TYPE_PTR) {
		void* value;
		memcpy(&value, slot, sizeof(value));
		qd_ptr_retain(value);
		return qd_push_p(ctx, value);
	}
	if (tag == (int64_t)QD_STACK_TYPE_D128) {
		const qd_d128_t* box;
		memcpy(&box, slot, sizeof(box));
		return qd_push_d(ctx, *box);
	}
	int64_t value;
	memcpy(&value, slot, sizeof(value));
	return qd_push_i(ctx, value);
}

// Stack signature: ( address:ptr offset:i64 -- )
// Release whatever the slot holds and zero it
int usr_mem_clear_any(qd_context* ctx) {
	void* address;
	int64_t offset;
	qd_args(ctx, "mem::clear_any", "pi", &address, &offset);
	char* slot = slot_at(ctx, "mem::clear_any", address, offset);
	mem_release_slot(slot);
	memset(slot, 0, 16);
	return 0;
}
