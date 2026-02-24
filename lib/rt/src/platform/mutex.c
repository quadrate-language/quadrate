// C11 threads implementation of mutex_platform
#include "mutex_platform.h"
#include <threads.h>
#include <stddef.h>

// The actual mutex structure wraps mtx_t
struct mutex_platform {
	mtx_t mutex;
};

size_t mutex_platform_size(void) {
	return sizeof(struct mutex_platform);
}

int mutex_platform_init(mutex_platform_t* mutex) {
	if (!mutex) {
		return -1;
	}
	return mtx_init(&mutex->mutex, mtx_plain) == thrd_success ? 0 : -1;
}

void mutex_platform_destroy(mutex_platform_t* mutex) {
	if (mutex) {
		mtx_destroy(&mutex->mutex);
	}
}

int mutex_platform_lock(mutex_platform_t* mutex) {
	if (!mutex) {
		return -1;
	}
	return mtx_lock(&mutex->mutex) == thrd_success ? 0 : -1;
}

int mutex_platform_unlock(mutex_platform_t* mutex) {
	if (!mutex) {
		return -1;
	}
	return mtx_unlock(&mutex->mutex) == thrd_success ? 0 : -1;
}
