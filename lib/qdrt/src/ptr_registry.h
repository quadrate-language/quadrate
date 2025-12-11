/**
 * @file ptr_registry.h
 * @brief Thread-safe pointer registry for tracking valid pointers
 *
 * This provides a hash-table based registry for tracking pointers,
 * used by array and struct implementations to validate pointers.
 */

#ifndef QD_PTR_REGISTRY_H
#define QD_PTR_REGISTRY_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define PTR_REGISTRY_SIZE 1024

typedef struct ptr_registry_entry {
	void* ptr;
	struct ptr_registry_entry* next;
} ptr_registry_entry_t;

typedef struct ptr_registry {
	ptr_registry_entry_t* buckets[PTR_REGISTRY_SIZE];
	pthread_mutex_t mutex;
} ptr_registry_t;

/* Initialize a registry (call once at startup or use static initializer) */
static inline void ptr_registry_init(ptr_registry_t* reg) {
	for (size_t i = 0; i < PTR_REGISTRY_SIZE; i++) {
		reg->buckets[i] = NULL;
	}
	pthread_mutex_init(&reg->mutex, NULL);
}

/* Static initializer for global registries */
#define PTR_REGISTRY_INITIALIZER { .buckets = {NULL}, .mutex = PTHREAD_MUTEX_INITIALIZER }

static inline size_t ptr_registry_hash(const void* ptr) {
	uintptr_t val = (uintptr_t)ptr;
	val = val ^ (val >> 16);
	return val % PTR_REGISTRY_SIZE;
}

static inline void ptr_registry_add(ptr_registry_t* reg, void* ptr) {
	pthread_mutex_lock(&reg->mutex);
	size_t idx = ptr_registry_hash(ptr);
	ptr_registry_entry_t* entry = (ptr_registry_entry_t*)malloc(sizeof(ptr_registry_entry_t));
	if (entry) {
		entry->ptr = ptr;
		entry->next = reg->buckets[idx];
		reg->buckets[idx] = entry;
	}
	pthread_mutex_unlock(&reg->mutex);
}

static inline int ptr_registry_contains(ptr_registry_t* reg, const void* ptr) {
	pthread_mutex_lock(&reg->mutex);
	size_t idx = ptr_registry_hash(ptr);
	ptr_registry_entry_t* entry = reg->buckets[idx];
	while (entry) {
		if (entry->ptr == ptr) {
			pthread_mutex_unlock(&reg->mutex);
			return 1;
		}
		entry = entry->next;
	}
	pthread_mutex_unlock(&reg->mutex);
	return 0;
}

static inline void ptr_registry_remove(ptr_registry_t* reg, void* ptr) {
	pthread_mutex_lock(&reg->mutex);
	size_t idx = ptr_registry_hash(ptr);
	ptr_registry_entry_t** pp = &reg->buckets[idx];
	while (*pp) {
		if ((*pp)->ptr == ptr) {
			ptr_registry_entry_t* to_free = *pp;
			*pp = (*pp)->next;
			free(to_free);
			pthread_mutex_unlock(&reg->mutex);
			return;
		}
		pp = &(*pp)->next;
	}
	pthread_mutex_unlock(&reg->mutex);
}

#endif /* QD_PTR_REGISTRY_H */
