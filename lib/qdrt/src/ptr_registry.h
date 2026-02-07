/**
 * @file ptr_registry.h
 * @brief Thread-safe pointer registry for tracking valid pointers
 *
 * This provides a hash-table based registry for tracking pointers,
 * used by array and struct implementations to validate pointers.
 */

#ifndef QD_PTR_REGISTRY_H
#define QD_PTR_REGISTRY_H

#include "platform/mutex_platform.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define PTR_REGISTRY_SIZE 1024

// Storage size for platform mutex (must be >= actual mutex size on all platforms)
// C11 mtx_t is typically 40 bytes on Linux x86_64
#define PTR_REGISTRY_MUTEX_STORAGE_SIZE 64

typedef struct ptr_registry_entry {
	void* ptr;
	struct ptr_registry_entry* next;
} ptr_registry_entry_t;

typedef struct ptr_registry {
	ptr_registry_entry_t* buckets[PTR_REGISTRY_SIZE];
	// Storage for platform-specific mutex
	// Aligned to 8 bytes for portability
	_Alignas(8) char mutex_storage[PTR_REGISTRY_MUTEX_STORAGE_SIZE];
	int initialized;
} ptr_registry_t;

/* Initialize a registry (call once at startup) */
static inline void ptr_registry_init(ptr_registry_t* reg) {
	for (size_t i = 0; i < PTR_REGISTRY_SIZE; i++) {
		reg->buckets[i] = NULL;
	}
	mutex_platform_init((mutex_platform_t*)reg->mutex_storage);
	reg->initialized = 1;
}

/* Destroy a registry */
static inline void ptr_registry_destroy(ptr_registry_t* reg) {
	if (reg->initialized) {
		mutex_platform_destroy((mutex_platform_t*)reg->mutex_storage);
		reg->initialized = 0;
	}
}

/* Static initializer for global registries (requires ptr_registry_init call) */
#define PTR_REGISTRY_INITIALIZER {.buckets = {NULL}, .mutex_storage = {0}, .initialized = 0}

static inline size_t ptr_registry_hash(const void* ptr) {
	uintptr_t val = (uintptr_t)ptr;
	val = val ^ (val >> 16);
	return val % PTR_REGISTRY_SIZE;
}

/* Ensure registry is initialized (lazy init for static registries)
 * Uses atomic CAS to safely bootstrap the init mutex */
static inline void ptr_registry_ensure_init(ptr_registry_t* reg) {
	if (!reg->initialized) {
		// Bootstrap: safely initialize the static init mutex using atomic CAS
		// States: 0 = uninitialized, 1 = initializing, 2 = ready
		static _Alignas(8) char init_mutex_storage[PTR_REGISTRY_MUTEX_STORAGE_SIZE] = {0};
		static atomic_int init_mutex_state = 0;
		int expected = 0;
		if (atomic_compare_exchange_strong(&init_mutex_state, &expected, 1)) {
			// Won the race: initialize the mutex
			mutex_platform_init((mutex_platform_t*)init_mutex_storage);
			atomic_store(&init_mutex_state, 2);
		} else {
			// Another thread is initializing or has finished; wait for completion
			while (atomic_load(&init_mutex_state) != 2) {
				// spin
			}
		}
		mutex_platform_lock((mutex_platform_t*)init_mutex_storage);
		// Double-check after acquiring lock
		if (!reg->initialized) {
			ptr_registry_init(reg);
		}
		mutex_platform_unlock((mutex_platform_t*)init_mutex_storage);
	}
}

static inline void ptr_registry_add(ptr_registry_t* reg, void* ptr) {
	ptr_registry_ensure_init(reg);
	mutex_platform_t* mutex = (mutex_platform_t*)reg->mutex_storage;
	mutex_platform_lock(mutex);
	size_t idx = ptr_registry_hash(ptr);
	ptr_registry_entry_t* entry = (ptr_registry_entry_t*)malloc(sizeof(ptr_registry_entry_t));
	if (entry) {
		entry->ptr = ptr;
		entry->next = reg->buckets[idx];
		reg->buckets[idx] = entry;
	}
	mutex_platform_unlock(mutex);
}

static inline int ptr_registry_contains(ptr_registry_t* reg, const void* ptr) {
	ptr_registry_ensure_init(reg);
	mutex_platform_t* mutex = (mutex_platform_t*)reg->mutex_storage;
	mutex_platform_lock(mutex);
	size_t idx = ptr_registry_hash(ptr);
	ptr_registry_entry_t* entry = reg->buckets[idx];
	while (entry) {
		if (entry->ptr == ptr) {
			mutex_platform_unlock(mutex);
			return 1;
		}
		entry = entry->next;
	}
	mutex_platform_unlock(mutex);
	return 0;
}

static inline void ptr_registry_remove(ptr_registry_t* reg, void* ptr) {
	ptr_registry_ensure_init(reg);
	mutex_platform_t* mutex = (mutex_platform_t*)reg->mutex_storage;
	mutex_platform_lock(mutex);
	size_t idx = ptr_registry_hash(ptr);
	ptr_registry_entry_t** pp = &reg->buckets[idx];
	while (*pp) {
		if ((*pp)->ptr == ptr) {
			ptr_registry_entry_t* to_free = *pp;
			*pp = (*pp)->next;
			free(to_free);
			mutex_platform_unlock(mutex);
			return;
		}
		pp = &(*pp)->next;
	}
	mutex_platform_unlock(mutex);
}

#endif /* QD_PTR_REGISTRY_H */
