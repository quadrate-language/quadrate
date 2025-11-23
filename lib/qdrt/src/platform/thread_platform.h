#ifndef QDRT_THREAD_PLATFORM_H
#define QDRT_THREAD_PLATFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Platform-agnostic thread handle type
typedef void* thread_handle_t;

// Thread function signature
typedef void* (*thread_func_t)(void*);

// Thread error codes
#define THREAD_SUCCESS 0
#define THREAD_ERROR -1

// Create a new thread
// Returns thread handle on success, NULL on failure
thread_handle_t thread_platform_create(thread_func_t func, void* arg);

// Wait for a thread to complete (join)
// Returns THREAD_SUCCESS on success, THREAD_ERROR on failure
int thread_platform_join(thread_handle_t handle);

// Detach a thread (allow it to run independently)
// Returns THREAD_SUCCESS on success, THREAD_ERROR on failure
int thread_platform_detach(thread_handle_t handle);

// Sleep for specified milliseconds
void thread_platform_sleep_ms(int milliseconds);

#ifdef __cplusplus
}
#endif

#endif // QDRT_THREAD_PLATFORM_H
