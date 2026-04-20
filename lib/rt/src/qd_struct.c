/**
 * @file qd_struct.c
 * @brief Reference-counted struct implementation for Quadrate runtime
 */

#include "quadrate/rt/qd_struct.h"
#include "quadrate/rt/array.h"
#include "ptr_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>

// Global registry of valid struct pointers
static ptr_registry_t struct_registry = PTR_REGISTRY_INITIALIZER;

// Struct operations

qd_struct_header_t* qd_struct_get_header(void* struct_ptr) {
	// Cast through void* to suppress alignment warnings - the header
	// is guaranteed to be aligned since malloc returns aligned memory
	// and header size is a multiple of 8
	char* byte_ptr = (char*)struct_ptr;
	void* header_ptr = byte_ptr - sizeof(qd_struct_header_t);
	return (qd_struct_header_t*)header_ptr;
}

int qd_struct_is_valid(const void* struct_ptr) {
	if (struct_ptr == NULL) {
		return 0;
	}
	// Use registry to safely check if this is a valid struct
	return ptr_registry_contains(&struct_registry, struct_ptr) ? 1 : 0;
}

void* qd_struct_alloc(size_t size, qd_struct_destructor_fn destructor) {
	// Allocate header + struct data
	void* mem = malloc(sizeof(qd_struct_header_t) + size);
	if (mem == NULL) {
		return NULL;
	}

	// Initialize header
	qd_struct_header_t* header = (qd_struct_header_t*)mem;
	header->magic = QD_STRUCT_MAGIC;
	atomic_init(&header->refcount, 1);
	header->destructor = destructor;

	// Return pointer to struct data (after header)
	void* struct_ptr = (char*)mem + sizeof(qd_struct_header_t);

	// Zero the struct body. Required for sized-integer fields (i8/i16/i32/u8/u16/u32)
	// that occupy only part of an 8-byte-aligned slot: the unused padding would
	// otherwise be undefined, and the optimizer can fold trunc/zext chains
	// through the undef bits and return surprising values.
	memset(struct_ptr, 0, size);

	// Register this struct pointer
	ptr_registry_add(&struct_registry, struct_ptr);

	return struct_ptr;
}

void* qd_struct_retain(void* struct_ptr) {
	if (struct_ptr == NULL) {
		return NULL;
	}

	// Check registry to verify this is a valid struct (safe for any pointer)
	if (!ptr_registry_contains(&struct_registry, struct_ptr)) {
		return struct_ptr;
	}

	qd_struct_header_t* header = qd_struct_get_header(struct_ptr);
	atomic_fetch_add(&header->refcount, 1);
	return struct_ptr;
}

void qd_struct_release(void* struct_ptr) {
	if (struct_ptr == NULL) {
		return;
	}

	// Check registry to verify this is a valid struct (safe for any pointer)
	if (!ptr_registry_contains(&struct_registry, struct_ptr)) {
		return;
	}

	qd_struct_header_t* header = qd_struct_get_header(struct_ptr);
	size_t old_count = atomic_fetch_sub(&header->refcount, 1);

	if (old_count == 1) {
		// Last reference - remove from registry, call destructor, then free
		ptr_registry_remove(&struct_registry, struct_ptr);

		if (header->destructor != NULL) {
			header->destructor(struct_ptr);
		}
		header->magic = 0; // Clear magic to prevent double-free
		free(header); // Free from the header pointer, not struct_ptr
	}
}

size_t qd_struct_refcount(const void* struct_ptr) {
	if (struct_ptr == NULL) {
		return 0;
	}

	// Check registry first
	if (!ptr_registry_contains(&struct_registry, struct_ptr)) {
		return 0;
	}

	const char* byte_ptr = (const char*)struct_ptr;
	const void* header_ptr = byte_ptr - sizeof(qd_struct_header_t);
	const qd_struct_header_t* header = (const qd_struct_header_t*)header_ptr;
	return atomic_load(&header->refcount);
}

// Generic pointer retain/release (works for both arrays and structs)

void* qd_ptr_retain(void* ptr) {
	if (ptr == NULL) {
		return NULL;
	}

	// Check if it's an array first (faster check via magic number)
	if (qd_array_is_valid(ptr)) {
		qd_array_retain((qd_array_t*)ptr);
		return ptr;
	}

	// Otherwise try struct retain (checks registry)
	return qd_struct_retain(ptr);
}

void qd_ptr_release(void* ptr) {
	if (ptr == NULL) {
		return;
	}

	// Check if it's an array first (faster check via magic number)
	if (qd_array_is_valid(ptr)) {
		qd_array_release((qd_array_t*)ptr);
		return;
	}

	// Otherwise try struct release (checks registry)
	qd_struct_release(ptr);
}
