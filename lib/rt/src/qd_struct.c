/**
 * @file qd_struct.c
 * @brief Reference-counted struct implementation for Quadrate runtime
 */

#include "quadrate/rt/qd_struct.h"
#include "quadrate/rt/array.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Struct operations

qd_struct_header_t* qd_struct_get_header(void* struct_ptr) {
	// Cast through void* to suppress alignment warnings - the header
	// is guaranteed to be aligned since malloc returns aligned memory
	// and header size is a multiple of 8
	char* byte_ptr = (char*)struct_ptr;
	void* header_ptr = byte_ptr - sizeof(qd_struct_header_t);
	return (qd_struct_header_t*)header_ptr;
}

void* qd_struct_alloc(size_t size, qd_struct_destructor_fn destructor) {
	// Allocate header + struct data
	void* mem = malloc(sizeof(qd_struct_header_t) + size);
	if (mem == NULL) {
		return NULL;
	}

	// Initialize header
	qd_struct_header_t* header = (qd_struct_header_t*)mem;
	// Truncated where size_t is 32 bits; it is a marker, never compared whole
	header->magic = (size_t)QD_STRUCT_MAGIC;
	atomic_init(&header->refcount, 1);
	header->destructor = destructor;

	// Return pointer to struct data (after header)
	void* struct_ptr = (char*)mem + sizeof(qd_struct_header_t);

	// Zero the struct body. Required for sized-integer fields (i8/i16/i32/u8/u16/u32)
	// that occupy only part of an 8-byte-aligned slot: the unused padding would
	// otherwise be undefined, and the optimizer can fold trunc/zext chains
	// through the undef bits and return surprising values.
	memset(struct_ptr, 0, size);
	return struct_ptr;
}

// A struct is known by the slot or field that holds it, never looked up: there is no registry
// of them. The magic is checked all the same, and a freed struct's is cleared before it goes,
// so a pointer that is not a live struct is left alone rather than counted.
static bool struct_magic_ok(void* struct_ptr) {
	return qd_struct_get_header(struct_ptr)->magic == (size_t)QD_STRUCT_MAGIC;
}

void* qd_struct_retain(void* struct_ptr) {
	if (struct_ptr != NULL && struct_magic_ok(struct_ptr)) {
		atomic_fetch_add(&qd_struct_get_header(struct_ptr)->refcount, 1);
	}
	return struct_ptr;
}

void qd_struct_release(void* struct_ptr) {
	if (struct_ptr == NULL || !struct_magic_ok(struct_ptr)) {
		return;
	}
	qd_struct_header_t* header = qd_struct_get_header(struct_ptr);
	if (atomic_fetch_sub(&header->refcount, 1) == 1) {
		if (header->destructor != NULL) {
			header->destructor(struct_ptr);
		}
		header->magic = 0; // a stale pointer is no struct
		free(header);	   // the allocation starts at the header, not at the fields
	}
}
