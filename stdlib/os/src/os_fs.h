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

// Check if path is a directory
// Returns 1 if directory, 0 if not
int os_fs_is_dir(const char* path);

// Check if path is a regular file
// Returns 1 if regular file, 0 if not
int os_fs_is_file(const char* path);

// Create a directory
// Returns 0 on success, error code on failure
int os_fs_mkdir(const char* path);

// Create a directory and all parent directories (mkdir -p)
// Returns 0 on success, error code on failure
int os_fs_mkdir_p(const char* path);

// Remove a directory and all its contents recursively (rm -rf)
// Returns 0 on success, error code on failure
int os_fs_rmdir_r(const char* path);

// List directory contents
// Returns NULL-terminated array of strings on success, NULL on failure
// Caller must free the array and all strings using os_fs_free_list()
char** os_fs_list_dir(const char* path, size_t* count);

// Free the list returned by os_fs_list_dir()
void os_fs_free_list(char** list);

// Callback type for os_fs_walk
// path: full path to the file/directory
// is_dir: 1 if directory, 0 if file
// depth: recursion depth (0 for top-level)
// user_data: user-provided context
// Return 0 to continue, non-zero to stop
typedef int (*os_fs_walk_callback)(const char* path, int is_dir, int depth, void* user_data);

// Walk a directory tree recursively
// Returns 0 on success, error code on failure
int os_fs_walk(const char* path, os_fs_walk_callback callback, void* user_data);

// Glob pattern matching
// Returns NULL-terminated array of matching paths, NULL on failure
// Caller must free using os_fs_free_list()
char** os_fs_glob(const char* pattern, size_t* count);

#ifdef __cplusplus
}
#endif

#endif // QD_QDOS_OS_FS_H
