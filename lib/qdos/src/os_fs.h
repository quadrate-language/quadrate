// Cross-platform filesystem operations using C++17 std::filesystem
// C-compatible interface for use from os.c

#ifndef QD_QDOS_OS_FS_H
#define QD_QDOS_OS_FS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Check if a file or directory exists
// Returns 1 if exists, 0 if not
int os_fs_exists(const char* path);

// Create a directory
// Returns 0 on success, error code on failure
int os_fs_mkdir(const char* path);

// List directory contents
// Returns NULL-terminated array of strings on success, NULL on failure
// Caller must free the array and all strings using os_fs_free_list()
char** os_fs_list_dir(const char* path, size_t* count);

// Free the list returned by os_fs_list_dir()
void os_fs_free_list(char** list);

#ifdef __cplusplus
}
#endif

#endif // QD_QDOS_OS_FS_H
