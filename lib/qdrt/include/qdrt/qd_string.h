/**
 * @file qd_string.h
 * @brief Reference-counted string implementation for Quadrate runtime
 *
 * Provides efficient string management with copy-on-write semantics through
 * reference counting. Strings are shared when possible (e.g., during stack
 * duplication) and only freed when all references are released.
 */

#ifndef QD_QUADRATE_RUNTIME_STRING_H
#define QD_QUADRATE_RUNTIME_STRING_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
#include <atomic>
extern "C" {
typedef std::atomic<size_t> qd_atomic_size_t;
#else
#include <stdatomic.h>
typedef atomic_size_t qd_atomic_size_t;
#endif

/**
 * @brief Reference-counted string structure
 *
 * Strings are immutable and use atomic reference counting for thread-safety.
 * The reference count starts at 1 when created.
 */
typedef struct qd_string {
	char* data;              ///< Null-terminated string data (owned)
	size_t length;           ///< String length (cached, excluding null terminator)
	qd_atomic_size_t refcount;  ///< Atomic reference count
} qd_string_t;

/**
 * @brief Create a new reference-counted string
 *
 * Creates a new string with an initial reference count of 1.
 * The input string is copied.
 *
 * @param str Source string to copy (must not be NULL)
 * @return Pointer to new qd_string_t, or NULL on allocation failure
 *
 * @note The caller owns the initial reference and must call qd_string_release()
 * @note Returns NULL if str is NULL or allocation fails
 */
qd_string_t* qd_string_create(const char* str);

/**
 * @brief Create a reference-counted string with known length
 *
 * More efficient than qd_string_create() when length is already known.
 *
 * @param str Source string to copy (must not be NULL)
 * @param length Length of the string (excluding null terminator)
 * @return Pointer to new qd_string_t, or NULL on allocation failure
 *
 * @note The caller owns the initial reference and must call qd_string_release()
 */
qd_string_t* qd_string_create_with_length(const char* str, size_t length);

/**
 * @brief Increment the reference count of a string
 *
 * Increases the reference count by 1, indicating that another owner
 * now shares ownership of the string.
 *
 * @param str String to retain (can be NULL - no-op)
 * @return The same string pointer for convenience
 *
 * @note Thread-safe: uses atomic operations
 * @note If str is NULL, this is a no-op and returns NULL
 */
qd_string_t* qd_string_retain(qd_string_t* str);

/**
 * @brief Decrement the reference count and free if zero
 *
 * Decreases the reference count by 1. If the count reaches zero,
 * the string data and structure are freed.
 *
 * @param str String to release (can be NULL - no-op)
 *
 * @note Thread-safe: uses atomic operations
 * @note If str is NULL, this is a no-op
 * @note After calling this, the pointer should be considered invalid
 *       unless you know other references exist
 */
void qd_string_release(qd_string_t* str);

/**
 * @brief Get the current reference count
 *
 * @param str String to query (must not be NULL)
 * @return Current reference count
 *
 * @note For debugging/testing purposes only
 * @note The returned value may be stale in multithreaded contexts
 */
size_t qd_string_refcount(const qd_string_t* str);

/**
 * @brief Get the string data
 *
 * @param str String to query (must not be NULL)
 * @return Pointer to null-terminated string data
 *
 * @note The returned pointer is valid as long as the string is retained
 * @note Do not free the returned pointer
 */
const char* qd_string_data(const qd_string_t* str);

/**
 * @brief Get the string length
 *
 * @param str String to query (must not be NULL)
 * @return Length of the string (excluding null terminator)
 */
size_t qd_string_length(const qd_string_t* str);

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_STRING_H
