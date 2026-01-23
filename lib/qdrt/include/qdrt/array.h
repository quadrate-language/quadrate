/**
 * @file array.h
 * @brief Dynamic array support for Quadrate runtime
 *
 * Provides reference-counted dynamic arrays with type tagging.
 * Arrays store their element type and manage memory automatically.
 */

#ifndef QD_QUADRATE_RUNTIME_ARRAY_H
#define QD_QUADRATE_RUNTIME_ARRAY_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>
#include <qdrt/stack.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <atomic>
typedef std::atomic<size_t> qd_array_atomic_size_t;
extern "C" {
#else
#include <stdatomic.h>
typedef atomic_size_t qd_array_atomic_size_t;
#endif

/**
 * @brief Array element type (matches stack element types)
 */
typedef enum {
	QD_ARRAY_TYPE_INT = 0,	 ///< 64-bit signed integer
	QD_ARRAY_TYPE_FLOAT = 1, ///< Double-precision floating point
	QD_ARRAY_TYPE_STR = 2,	 ///< Reference-counted string
	QD_ARRAY_TYPE_PTR = 3	 ///< Generic pointer
} qd_array_type;

/**
 * @brief Magic number for array validation
 */
#define QD_ARRAY_MAGIC 0x51444152UL // "QDAR" in hex

/**
 * @brief Default initial capacity for arrays
 */
#define QD_ARRAY_DEFAULT_CAPACITY 8

/**
 * @brief Reference-counted dynamic array structure
 *
 * Arrays are heap-allocated and reference-counted.
 * When the reference count reaches 0, the array and its contents are freed.
 * Arrays use atomic reference counting for thread-safety.
 */
typedef struct qd_array {
	size_t magic;					 ///< Magic number for validation (QD_ARRAY_MAGIC)
	qd_array_atomic_size_t refcount; ///< Atomic reference count
	size_t length;					 ///< Number of elements
	size_t capacity;				 ///< Allocated capacity
	qd_array_type elemType;			 ///< Type of elements in the array

	union {
		int64_t* i; ///< Integer array data
		double* f;	///< Float array data
		void** p;	///< Pointer array data (includes strings)
	} data;
} qd_array_t;

/**
 * @brief Check if a pointer is a valid array
 *
 * @param ptr Pointer to check
 * @return 1 if valid array, 0 otherwise
 */
int qd_array_is_valid(const void* ptr);

/**
 * @brief Create a new array with the specified capacity and element type
 *
 * @param capacity Initial capacity (number of elements)
 * @param elemType Type of elements the array will hold
 * @return Pointer to the new array, or NULL on allocation failure
 *
 * @note The caller is responsible for releasing the array with qd_array_release()
 */
qd_array_t* qd_array_create(size_t capacity, qd_array_type elemType);

/**
 * @brief Retain (increment reference count of) an array
 *
 * @param arr Array to retain
 * @note Thread-safe: uses atomic operations
 */
void qd_array_retain(qd_array_t* arr);

/**
 * @brief Release (decrement reference count of) an array
 *
 * If the reference count reaches 0, the array and its contents are freed.
 * For string arrays, all string references are released.
 *
 * @param arr Array to release (can be NULL)
 * @note Thread-safe: uses atomic operations
 */
void qd_array_release(qd_array_t* arr);

/**
 * @brief Get the length of an array
 *
 * @param arr Array to query
 * @return Number of elements in the array
 */
size_t qd_array_length(const qd_array_t* arr);

/**
 * @brief Get an integer element from an array
 *
 * @param arr Array to access
 * @param index Index of element (0-based)
 * @param[out] value Receives the element value
 * @return 0 on success, -1 on error (index out of bounds or type mismatch)
 */
int qd_array_get_int(const qd_array_t* arr, size_t index, int64_t* value);

/**
 * @brief Get a float element from an array
 *
 * @param arr Array to access
 * @param index Index of element (0-based)
 * @param[out] value Receives the element value
 * @return 0 on success, -1 on error (index out of bounds or type mismatch)
 */
int qd_array_get_float(const qd_array_t* arr, size_t index, double* value);

/**
 * @brief Get a pointer element from an array
 *
 * @param arr Array to access
 * @param index Index of element (0-based)
 * @param[out] value Receives the element value
 * @return 0 on success, -1 on error (index out of bounds or type mismatch)
 */
int qd_array_get_ptr(const qd_array_t* arr, size_t index, void** value);

/**
 * @brief Set an integer element in an array
 *
 * @param arr Array to modify
 * @param index Index of element (0-based)
 * @param value Value to set
 * @return 0 on success, -1 on error (index out of bounds or type mismatch)
 */
int qd_array_set_int(qd_array_t* arr, size_t index, int64_t value);

/**
 * @brief Set a float element in an array
 *
 * @param arr Array to modify
 * @param index Index of element (0-based)
 * @param value Value to set
 * @return 0 on success, -1 on error (index out of bounds or type mismatch)
 */
int qd_array_set_float(qd_array_t* arr, size_t index, double value);

/**
 * @brief Set a pointer element in an array
 *
 * @param arr Array to modify
 * @param index Index of element (0-based)
 * @param value Value to set
 * @return 0 on success, -1 on error (index out of bounds or type mismatch)
 */
int qd_array_set_ptr(qd_array_t* arr, size_t index, void* value);

/**
 * @brief Push an integer element to the end of an array
 *
 * @param arr Array to modify
 * @param value Value to push
 * @return 0 on success, -1 on error (allocation failure or type mismatch)
 */
int qd_array_push_int(qd_array_t* arr, int64_t value);

/**
 * @brief Push a float element to the end of an array
 *
 * @param arr Array to modify
 * @param value Value to push
 * @return 0 on success, -1 on error (allocation failure or type mismatch)
 */
int qd_array_push_float(qd_array_t* arr, double value);

/**
 * @brief Push a pointer element to the end of an array
 *
 * @param arr Array to modify
 * @param value Value to push
 * @return 0 on success, -1 on error (allocation failure or type mismatch)
 */
int qd_array_push_ptr(qd_array_t* arr, void* value);

/**
 * @defgroup ArrayStackOps Array Stack Operations
 * @brief Stack-based array operations for Quadrate runtime
 * @{
 */

/**
 * @brief Get array length and push onto stack
 *
 * Stack: ( array:ptr -- length:i64 )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_len(qd_context* ctx);

/**
 * @brief Get element at index from array
 *
 * Stack: ( array:ptr index:i64 -- value )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_nth(qd_context* ctx);

/**
 * @brief Create a new integer array
 *
 * Stack: ( size:i64 -- array:ptr )
 *
 * Creates a new array with `size` integer elements, all initialized to 0.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_makei(qd_context* ctx);

/**
 * @brief Create a new float array
 *
 * Stack: ( size:i64 -- array:ptr )
 *
 * Creates a new array with `size` float elements, all initialized to 0.0.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_makef(qd_context* ctx);

/**
 * @brief Create a new string array
 *
 * Stack: ( size:i64 -- array:ptr )
 *
 * Creates a new array with `size` string elements, all initialized to empty strings.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_makes(qd_context* ctx);

/**
 * @brief Create a new pointer array
 *
 * Stack: ( size:i64 -- array:ptr )
 *
 * Creates a new array with `size` pointer elements, all initialized to null.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_makep(qd_context* ctx);

/**
 * @brief Append element to array
 *
 * Stack: ( array:ptr value -- array:ptr )
 *
 * Appends the value to the end of the array and returns the array.
 * The value type must match the array element type.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_append(qd_context* ctx);

/**
 * @brief Set element at index in array
 *
 * Stack: ( array:ptr index:i64 value -- )
 *
 * Sets the element at the given index. The value type must match the array element type.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_set(qd_context* ctx);

/** @} */ // end of ArrayStackOps group

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_ARRAY_H
