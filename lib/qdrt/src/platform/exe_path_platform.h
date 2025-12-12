#ifndef QD_QDRT_EXE_PATH_PLATFORM_H
#define QD_QDRT_EXE_PATH_PLATFORM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the path to the currently running executable.
 *
 * @param buffer Buffer to store the path
 * @param buffer_size Size of the buffer
 * @return Length of the path on success, -1 on failure
 *
 * Note: The returned path is NOT null-terminated if the buffer is exactly filled.
 *       Always check return value < buffer_size before treating as C string.
 */
int exe_path_platform_get(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // QD_QDRT_EXE_PATH_PLATFORM_H
