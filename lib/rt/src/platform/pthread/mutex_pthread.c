// pthreads implementation of mutex_platform, for a libc with no C11 threads
// (newlib, as on ESP-IDF)
#include "../mutex_platform.h"
#include <pthread.h>
#include <stddef.h>

struct mutex_platform {
	pthread_mutex_t mutex;
};

size_t mutex_platform_size(void) {
	return sizeof(struct mutex_platform);
}

int mutex_platform_init(mutex_platform_t* mutex) {
	if (!mutex) {
		return -1;
	}
	return pthread_mutex_init(&mutex->mutex, NULL) == 0 ? 0 : -1;
}

void mutex_platform_destroy(mutex_platform_t* mutex) {
	if (mutex) {
		pthread_mutex_destroy(&mutex->mutex);
	}
}

int mutex_platform_lock(mutex_platform_t* mutex) {
	if (!mutex) {
		return -1;
	}
	return pthread_mutex_lock(&mutex->mutex) == 0 ? 0 : -1;
}

int mutex_platform_unlock(mutex_platform_t* mutex) {
	if (!mutex) {
		return -1;
	}
	return pthread_mutex_unlock(&mutex->mutex) == 0 ? 0 : -1;
}
