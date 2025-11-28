/**
 * @file qd_struct.h
 * @brief Reference-counted struct support for Quadrate runtime
 *
 * Provides automatic memory management for user-defined structs through
 * reference counting. Structs are allocated with a hidden header containing
 * the refcount and destructor pointer.
 */

#ifndef QD_QUADRATE_RUNTIME_STRUCT_H
#define QD_QUADRATE_RUNTIME_STRUCT_H

#include <stddef.h>

#ifdef __cplusplus
#include <atomic>
extern "C" {
typedef std::atomic<size_t> qd_struct_atomic_size_t;
#else
#include <stdatomic.h>
typedef atomic_size_t qd_struct_atomic_size_t;
#endif

/**
 * @brief Magic number to identify valid struct headers
 */
#define QD_STRUCT_MAGIC 0x5144535452554354ULL  // "QDSTRUCT" in hex

/**
 * @brief Destructor function type for structs
 *
 * Called when the refcount reaches zero to cleanup nested structs and strings.
 * The destructor receives the struct data pointer (not the header).
 *
 * @param struct_ptr Pointer to the struct data
 */
typedef void (*qd_struct_destructor_fn)(void* struct_ptr);

/**
 * @brief Header prepended to all reference-counted structs
 *
 * This header is hidden from user code - struct pointers point to the
 * data immediately after this header.
 */
typedef struct qd_struct_header {
	size_t magic;                       ///< Magic number for validation
	qd_struct_atomic_size_t refcount;   ///< Atomic reference count
	qd_struct_destructor_fn destructor; ///< Destructor function (can be NULL)
} qd_struct_header_t;

/**
 * @brief Check if a pointer is a valid struct pointer
 *
 * @param struct_ptr Pointer to check (can be NULL)
 * @return 1 if valid struct, 0 otherwise
 */
int qd_struct_is_valid(const void* struct_ptr);

/**
 * @brief Allocate a new reference-counted struct
 *
 * Allocates memory for the header plus the struct data.
 * Returns a pointer to the struct data (after the header).
 * Initial refcount is 1.
 *
 * @param size Size of the struct data (not including header)
 * @param destructor Destructor function to call when refcount reaches 0 (can be NULL)
 * @return Pointer to struct data, or NULL on allocation failure
 *
 * @note The caller owns the initial reference and must call qd_struct_release()
 */
void* qd_struct_alloc(size_t size, qd_struct_destructor_fn destructor);

/**
 * @brief Increment the reference count of a struct
 *
 * Increases the reference count by 1, indicating that another owner
 * now shares ownership of the struct.
 *
 * @param struct_ptr Pointer to struct data (can be NULL - no-op)
 * @return The same struct pointer for convenience
 *
 * @note Thread-safe: uses atomic operations
 */
void* qd_struct_retain(void* struct_ptr);

/**
 * @brief Decrement the reference count and free if zero
 *
 * Decreases the reference count by 1. If the count reaches zero,
 * the destructor is called (if set) and the memory is freed.
 *
 * @param struct_ptr Pointer to struct data (can be NULL - no-op)
 *
 * @note Thread-safe: uses atomic operations
 * @note After calling this, the pointer should be considered invalid
 *       unless you know other references exist
 */
void qd_struct_release(void* struct_ptr);

/**
 * @brief Get the current reference count of a struct
 *
 * @param struct_ptr Pointer to struct data (must not be NULL)
 * @return Current reference count
 *
 * @note For debugging/testing purposes only
 */
size_t qd_struct_refcount(const void* struct_ptr);

/**
 * @brief Get the header from a struct pointer
 *
 * Internal helper to access the header from the user-visible struct pointer.
 * The header is placed immediately before the struct data in memory.
 * Since malloc returns properly aligned memory and the header size is a
 * multiple of 8, the alignment is guaranteed to be correct.
 *
 * @param struct_ptr Pointer to struct data
 * @return Pointer to the header
 */
qd_struct_header_t* qd_struct_get_header(void* struct_ptr);

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_STRUCT_H
