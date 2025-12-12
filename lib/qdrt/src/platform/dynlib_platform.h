#ifndef QD_QDRT_DYNLIB_PLATFORM_H
#define QD_QDRT_DYNLIB_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type for dynamic libraries
typedef void* dynlib_handle_t;

/**
 * Load a dynamic library.
 *
 * @param path Path to the shared library
 * @return Handle to the library, or NULL on failure
 */
dynlib_handle_t dynlib_platform_open(const char* path);

/**
 * Get a symbol from a dynamic library.
 *
 * @param handle Library handle from dynlib_platform_open
 * @param symbol Name of the symbol to look up
 * @return Pointer to the symbol, or NULL if not found
 */
void* dynlib_platform_symbol(dynlib_handle_t handle, const char* symbol);

/**
 * Close a dynamic library.
 *
 * @param handle Library handle from dynlib_platform_open
 */
void dynlib_platform_close(dynlib_handle_t handle);

/**
 * Get the last error message.
 *
 * @return Error message string, or NULL if no error
 */
const char* dynlib_platform_error(void);

#ifdef __cplusplus
}
#endif

#endif // QD_QDRT_DYNLIB_PLATFORM_H
