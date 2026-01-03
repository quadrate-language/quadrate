// C11 threads implementation
#include "thread_platform.h"
#include <threads.h>
#include <stdlib.h>
#include <time.h>

// Create a new thread
thread_handle_t thread_platform_create(thread_func_t func, void* arg) {
	thrd_t* thread = malloc(sizeof(thrd_t));
	if (!thread) {
		return NULL;
	}

	int result = thrd_create(thread, func, arg);
	if (result != thrd_success) {
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

	thrd_t* thread = (thrd_t*)handle;
	int result = thrd_join(*thread, NULL);
	free(thread);

	return (result == thrd_success) ? THREAD_SUCCESS : THREAD_ERROR;
}

// Detach a thread (allow it to run independently)
int thread_platform_detach(thread_handle_t handle) {
	if (!handle) {
		return THREAD_ERROR;
	}

	thrd_t* thread = (thrd_t*)handle;
	int result = thrd_detach(*thread);
	free(thread);

	return (result == thrd_success) ? THREAD_SUCCESS : THREAD_ERROR;
}

// Sleep for specified milliseconds
void thread_platform_sleep_ms(int milliseconds) {
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000L;
	thrd_sleep(&ts, NULL);
}
