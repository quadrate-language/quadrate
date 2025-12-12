#ifndef QD_QDRT_MUTEX_PLATFORM_H
#define QD_QDRT_MUTEX_PLATFORM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declare the platform-specific mutex type
// Each platform implementation defines this
struct mutex_platform;
typedef struct mutex_platform mutex_platform_t;

/**
 * Get the size of the mutex structure for this platform.
 * Used for static allocation.
 */
size_t mutex_platform_size(void);

/**
 * Initialize a mutex.
 *
 * @param mutex Pointer to mutex storage (must be at least mutex_platform_size() bytes)
 * @return 0 on success, -1 on failure
 */
int mutex_platform_init(mutex_platform_t* mutex);

/**
 * Destroy a mutex.
 *
 * @param mutex Pointer to an initialized mutex
 */
void mutex_platform_destroy(mutex_platform_t* mutex);

/**
 * Lock a mutex.
 *
 * @param mutex Pointer to an initialized mutex
 * @return 0 on success, -1 on failure
 */
int mutex_platform_lock(mutex_platform_t* mutex);

/**
 * Unlock a mutex.
 *
 * @param mutex Pointer to an initialized mutex
 * @return 0 on success, -1 on failure
 */
int mutex_platform_unlock(mutex_platform_t* mutex);

#ifdef __cplusplus
}
#endif

#endif // QD_QDRT_MUTEX_PLATFORM_H
