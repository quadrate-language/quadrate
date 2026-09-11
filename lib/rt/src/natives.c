// Native function registry, held on the context so both execution tiers see
// the same registrations. lib/qd may link the shared runtime while lib/interp
// links the static one, so this cannot live at file scope.

#define _POSIX_C_SOURCE 200809L

#include "runtime_internal.h"

#include <quadrate/rt/context.h>
#include <quadrate/rt/runtime.h>

#include <stdlib.h>
#include <string.h>

// Open addressing with linear probing. Capacity is a power of two so the
// bucket index is a mask. No tombstones: entries are only removed wholesale
// by qd_native_clear().
#define QD_NATIVES_MIN_CAPACITY 16

typedef struct {
	char* name; // NULL marks an empty slot
	qd_native_callback fn;
	void* userdata;
	size_t arity;
} qd_native_entry;

struct qd_native_registry {
	qd_native_entry* entries;
	size_t count;
	size_t capacity; // power of two, or 0 before the first insert
};

// FNV-1a
static uint64_t hash_name(const char* name) {
	uint64_t hash = 14695981039346656037ULL;
	for (const char* p = name; *p != '\0'; p++) {
		hash ^= (uint64_t)(unsigned char)*p;
		hash *= 1099511628211ULL;
	}
	return hash;
}

// The slot holding name, or the empty slot where it belongs. Never returns
// NULL: the table is grown before it can fill.
static qd_native_entry* slot_for(qd_native_entry* entries, size_t capacity, const char* name) {
	const size_t mask = capacity - 1;
	size_t index = (size_t)(hash_name(name) & (uint64_t)mask);

	while (entries[index].name != NULL && strcmp(entries[index].name, name) != 0) {
		index = (index + 1) & mask;
	}
	return &entries[index];
}

// Grow and rehash. Names are moved, not copied.
static bool grow(struct qd_native_registry* reg) {
	const size_t capacity = (reg->capacity == 0) ? QD_NATIVES_MIN_CAPACITY : reg->capacity * 2;

	qd_native_entry* entries = (qd_native_entry*)calloc(capacity, sizeof(qd_native_entry));
	if (entries == NULL) {
		return false;
	}

	for (size_t i = 0; i < reg->capacity; i++) {
		if (reg->entries[i].name != NULL) {
			*slot_for(entries, capacity, reg->entries[i].name) = reg->entries[i];
		}
	}

	free(reg->entries);
	reg->entries = entries;
	reg->capacity = capacity;
	return true;
}

// Inputs in a stack effect: "(a:i64 b:i64 -- r:i64)" has two. Returns 0 for a
// signature that cannot be read, which disables the caller's arity check.
static size_t count_inputs(const char* signature) {
	if (signature == NULL) {
		return 0;
	}

	const char* end = strstr(signature, "--");
	size_t count = 0;
	bool in_word = false;

	for (const char* p = signature; *p != '\0' && (end == NULL || p < end); p++) {
		const bool separator = (*p == ' ' || *p == '\t' || *p == '(' || *p == ')' || *p == ',');
		if (separator) {
			in_word = false;
		} else if (!in_word) {
			in_word = true;
			count++;
		}
	}
	return count;
}

bool qd_native_register(qd_context* ctx, const char* name, const char* signature, qd_native_callback fn,
		void* userdata) {
	if (ctx == NULL || name == NULL || *name == '\0' || fn == NULL) {
		return false;
	}

	if (ctx->natives == NULL) {
		ctx->natives = (struct qd_native_registry*)calloc(1, sizeof(struct qd_native_registry));
		if (ctx->natives == NULL) {
			return false;
		}
	}
	struct qd_native_registry* reg = ctx->natives;

	// Grow at 70% load, before probing can run long
	if (reg->capacity == 0 || (reg->count + 1) * 10 > reg->capacity * 7) {
		if (!grow(reg)) {
			return false;
		}
	}

	qd_native_entry* slot = slot_for(reg->entries, reg->capacity, name);

	if (slot->name == NULL) {
		slot->name = strdup(name);
		if (slot->name == NULL) {
			return false;
		}
		reg->count++;
	}

	// Re-registering a name replaces it
	slot->fn = fn;
	slot->userdata = userdata;
	slot->arity = count_inputs(signature);
	return true;
}

bool qd_native_lookup(const qd_context* ctx, const char* name, qd_native_callback* fn, void** userdata,
		size_t* arity) {
	if (ctx == NULL || ctx->natives == NULL || ctx->natives->capacity == 0 || name == NULL) {
		return false;
	}

	const qd_native_entry* slot = slot_for(ctx->natives->entries, ctx->natives->capacity, name);
	if (slot->name == NULL) {
		return false;
	}

	if (fn != NULL) {
		*fn = slot->fn;
	}
	if (userdata != NULL) {
		*userdata = slot->userdata;
	}
	if (arity != NULL) {
		*arity = slot->arity;
	}
	return true;
}

size_t qd_native_count(const qd_context* ctx) {
	return (ctx != NULL && ctx->natives != NULL) ? ctx->natives->count : 0;
}

void qd_native_clear(qd_context* ctx) {
	if (ctx == NULL || ctx->natives == NULL) {
		return;
	}

	for (size_t i = 0; i < ctx->natives->capacity; i++) {
		free(ctx->natives->entries[i].name);
	}
	free(ctx->natives->entries);
	free(ctx->natives);
	ctx->natives = NULL;
}
