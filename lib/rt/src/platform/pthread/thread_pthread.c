// pthreads implementation of thread_platform, for a libc with no C11 threads
// (newlib, as on ESP-IDF)
#define _POSIX_C_SOURCE 200809L
#include "../thread_platform.h"
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

// pthreads wants void* (*)(void*); the platform takes int (*)(void*)
typedef struct {
	pthread_t thread;
	thread_func_t func;
	void* arg;
} pthread_box;

static void* trampoline(void* p) {
	pthread_box* box = (pthread_box*)p;
	box->func(box->arg);
	return NULL;
}

thread_handle_t thread_platform_create(thread_func_t func, void* arg) {
	pthread_box* box = malloc(sizeof(pthread_box));
	if (!box) {
		return NULL;
	}

	box->func = func;
	box->arg = arg;
	if (pthread_create(&box->thread, NULL, trampoline, box) != 0) {
		free(box);
		return NULL;
	}

	return (thread_handle_t)box;
}

// The box is freed by whoever lets go of the thread. A detached thread still
// reads it once it starts, so detach leaves it -- one small leak per detached
// thread, against reading freed memory.
int thread_platform_join(thread_handle_t handle) {
	if (!handle) {
		return THREAD_ERROR;
	}

	pthread_box* box = (pthread_box*)handle;
	int result = pthread_join(box->thread, NULL);
	free(box);

	return (result == 0) ? THREAD_SUCCESS : THREAD_ERROR;
}

int thread_platform_detach(thread_handle_t handle) {
	if (!handle) {
		return THREAD_ERROR;
	}

	pthread_box* box = (pthread_box*)handle;
	return (pthread_detach(box->thread) == 0) ? THREAD_SUCCESS : THREAD_ERROR;
}

void thread_platform_sleep_ms(int milliseconds) {
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}
