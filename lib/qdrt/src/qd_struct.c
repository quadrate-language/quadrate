/**
 * @file qd_struct.c
 * @brief Reference-counted struct implementation for Quadrate runtime
 */

#include "qdrt/qd_struct.h"
#include "qdrt/array.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdbool.h>

// ============================================================================
// Global registry of valid struct pointers
// This allows safe retain/release on any pointer by checking registry membership
// ============================================================================

#define REGISTRY_SIZE 1024
#define REGISTRY_LOAD_FACTOR 0.75

typedef struct registry_entry {
	void* ptr;
	struct registry_entry* next;
} registry_entry_t;

static registry_entry_t* struct_registry[REGISTRY_SIZE];
static pthread_mutex_t registry_mutex = PTHREAD_MUTEX_INITIALIZER;
static size_t registry_count = 0;

static size_t ptr_hash(const void* ptr) {
	// Use pointer value as hash, shift to spread bits
	uintptr_t val = (uintptr_t)ptr;
	val = val ^ (val >> 16);
	return val % REGISTRY_SIZE;
}

static void registry_add(void* ptr) {
	pthread_mutex_lock(&registry_mutex);
	size_t idx = ptr_hash(ptr);
	registry_entry_t* entry = malloc(sizeof(registry_entry_t));
	if (entry) {
		entry->ptr = ptr;
		entry->next = struct_registry[idx];
		struct_registry[idx] = entry;
		registry_count++;
	}
	pthread_mutex_unlock(&registry_mutex);
}

static void registry_remove(void* ptr) {
	pthread_mutex_lock(&registry_mutex);
	size_t idx = ptr_hash(ptr);
	registry_entry_t** prev = &struct_registry[idx];
	registry_entry_t* curr = struct_registry[idx];
	while (curr) {
		if (curr->ptr == ptr) {
			*prev = curr->next;
			free(curr);
			registry_count--;
			break;
		}
		prev = &curr->next;
		curr = curr->next;
	}
	pthread_mutex_unlock(&registry_mutex);
}

static bool registry_contains(const void* ptr) {
	pthread_mutex_lock(&registry_mutex);
	size_t idx = ptr_hash(ptr);
	registry_entry_t* curr = struct_registry[idx];
	while (curr) {
		if (curr->ptr == ptr) {
			pthread_mutex_unlock(&registry_mutex);
			return true;
		}
		curr = curr->next;
	}
	pthread_mutex_unlock(&registry_mutex);
	return false;
}

// ============================================================================
// Struct operations
// ============================================================================

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
	return registry_contains(struct_ptr) ? 1 : 0;
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

	// Register this struct pointer
	registry_add(struct_ptr);

	return struct_ptr;
}

void* qd_struct_retain(void* struct_ptr) {
	if (struct_ptr == NULL) {
		return NULL;
	}

	// Check registry to verify this is a valid struct (safe for any pointer)
	if (!registry_contains(struct_ptr)) {
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
	if (!registry_contains(struct_ptr)) {
		return;
	}

	qd_struct_header_t* header = qd_struct_get_header(struct_ptr);
	size_t old_count = atomic_fetch_sub(&header->refcount, 1);

	if (old_count == 1) {
		// Last reference - remove from registry, call destructor, then free
		registry_remove(struct_ptr);

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
	if (!registry_contains(struct_ptr)) {
		return 0;
	}

	const char* byte_ptr = (const char*)struct_ptr;
	const void* header_ptr = byte_ptr - sizeof(qd_struct_header_t);
	const qd_struct_header_t* header = (const qd_struct_header_t*)header_ptr;
	return atomic_load(&header->refcount);
}
