// POSIX pthread implementation
#define _POSIX_C_SOURCE 200809L
#include "../thread_platform.h"
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

// Create a new thread
thread_handle_t thread_platform_create(thread_func_t func, void* arg) {
	pthread_t* thread = malloc(sizeof(pthread_t));
	if (!thread) {
		return NULL;
	}

	int result = pthread_create(thread, NULL, func, arg);
	if (result != 0) {
		free(thread);
		return NULL;
	}

	return (thread_handle_t)thread;
}

// Wait for a thread to complete (join)
int thread_platform_join(thread_handle_t handle) {
	if (!handle) {
		return THREAD_ERROR;
	}

	pthread_t* thread = (pthread_t*)handle;
	int result = pthread_join(*thread, NULL);
	free(thread);

	return (result == 0) ? THREAD_SUCCESS : THREAD_ERROR;
}

// Detach a thread (allow it to run independently)
int thread_platform_detach(thread_handle_t handle) {
	if (!handle) {
		return THREAD_ERROR;
	}

	pthread_t* thread = (pthread_t*)handle;
	int result = pthread_detach(*thread);
	free(thread);

	return (result == 0) ? THREAD_SUCCESS : THREAD_ERROR;
}

// Sleep for specified milliseconds
void thread_platform_sleep_ms(int milliseconds) {
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}
