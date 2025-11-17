/**
 * @file stack.h
 * @brief Type-safe stack data structure for Quadrate runtime
 *
 * Provides a dynamically-sized, type-safe stack with support for
 * integers, floats, pointers, and strings.
 */

#ifndef QD_QUADRATE_RUNTIME_STACK_H
#define QD_QUADRATE_RUNTIME_STACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Stack operation error codes
 */
typedef enum {
	QD_STACK_OK = 0,                  ///< Operation successful
	QD_STACK_ERR_INVALID_CAPACITY = 1, ///< Invalid capacity specified
	QD_STACK_ERR_OVERFLOW = 2,         ///< Stack overflow (capacity exceeded)
	QD_STACK_ERR_UNDERFLOW = 3,        ///< Stack underflow (pop from empty stack)
	QD_STACK_ERR_TYPE_MISMATCH = 4,    ///< Type mismatch in operation
	QD_STACK_ERR_NULL_POINTER = 5,     ///< Null pointer argument
	QD_STACK_ERR_ALLOC = 6            ///< Memory allocation failure
} qd_stack_error;

/**
 * @brief Stack element types
 *
 * Each element on the stack is tagged with one of these types,
 * enabling runtime type checking.
 */
typedef enum {
	QD_STACK_TYPE_INT,    ///< 64-bit signed integer
	QD_STACK_TYPE_FLOAT,  ///< Double-precision floating point
	QD_STACK_TYPE_PTR,    ///< Generic pointer
	QD_STACK_TYPE_STR     ///< Null-terminated string
} qd_stack_type;

/**
 * @brief A tagged union representing a single stack element
 *
 * Each stack element contains a value, its type tag, and an error taint flag
 * used for error propagation in Quadrate programs.
 */
typedef struct {
	union {
		int64_t i;  ///< Integer value
		double f;   ///< Float value
		void* p;    ///< Pointer value
		char* s;    ///< String value (owned by stack)
	} value;

	qd_stack_type type;        ///< Type of the stored value
	bool is_error_tainted;     ///< Error propagation flag
} qd_stack_element_t;

/**
 * @brief Opaque stack structure
 *
 * The internal structure is hidden to maintain encapsulation.
 * Use the provided functions to interact with the stack.
 */
typedef struct qd_stack qd_stack;

/**
 * @brief Initialize a new stack with the specified capacity
 *
 * @param[out] stack Pointer to receive the allocated stack
 * @param capacity Maximum number of elements the stack can hold
 * @return QD_STACK_OK on success, error code otherwise
 *
 * @note The caller is responsible for calling qd_stack_destroy() to free the stack
 */
qd_stack_error qd_stack_init(qd_stack** stack, size_t capacity);

/**
 * @brief Destroy a stack and free all associated resources
 *
 * @param stack Stack to destroy (can be NULL)
 *
 * @note Frees all string values owned by the stack
 */
void qd_stack_destroy(qd_stack* stack);

/**
 * @brief Clone a stack (deep copy)
 *
 * Creates a deep copy of the source stack, including all string values.
 * The cloned stack will have the same capacity and contents as the source.
 *
 * @param[out] dest Pointer to receive the cloned stack
 * @param src Source stack to clone
 * @return QD_STACK_OK on success, error code otherwise
 *
 * @note The caller is responsible for calling qd_stack_destroy() on the cloned stack
 * @note All strings in the stack are deep copied
 */
qd_stack_error qd_stack_clone(qd_stack** dest, const qd_stack* src);

/**
 * @brief Push a 64-bit integer onto the stack
 *
 * @param stack Target stack
 * @param value Integer value to push
 * @return QD_STACK_OK on success, QD_STACK_ERR_OVERFLOW if stack is full
 */
qd_stack_error qd_stack_push_int(qd_stack* stack, int64_t value);

/**
 * @brief Push a double-precision float onto the stack
 *
 * @param stack Target stack
 * @param value Float value to push
 * @return QD_STACK_OK on success, QD_STACK_ERR_OVERFLOW if stack is full
 */
qd_stack_error qd_stack_push_float(qd_stack* stack, double value);

/**
 * @brief Push a pointer onto the stack
 *
 * @param stack Target stack
 * @param value Pointer value to push
 * @return QD_STACK_OK on success, QD_STACK_ERR_OVERFLOW if stack is full
 *
 * @note The pointer is stored as-is; the stack does not take ownership
 */
qd_stack_error qd_stack_push_ptr(qd_stack* stack, void* value);

/**
 * @brief Push a string onto the stack
 *
 * @param stack Target stack
 * @param value String to push (will be copied)
 * @return QD_STACK_OK on success, error code otherwise
 *
 * @note The string is copied; the stack takes ownership of the copy
 */
qd_stack_error qd_stack_push_str(qd_stack* stack, const char* value);

/**
 * @brief Peek at the top element without removing it
 *
 * @param stack Source stack
 * @param[out] element Receives a copy of the top element
 * @return QD_STACK_OK on success, QD_STACK_ERR_UNDERFLOW if stack is empty
 */
qd_stack_error qd_stack_peek(qd_stack* stack, qd_stack_element_t* element);

/**
 * @brief Get element at a specific index (0 = bottom, size-1 = top)
 *
 * @param stack Source stack
 * @param index Index of element to retrieve
 * @param[out] element Receives a copy of the element
 * @return QD_STACK_OK on success, QD_STACK_ERR_UNDERFLOW if index is invalid
 */
qd_stack_error qd_stack_element(qd_stack* stack, size_t index, qd_stack_element_t* element);

/**
 * @brief Pop the top element from the stack
 *
 * @param stack Source stack
 * @param[out] element Receives the popped element
 * @return QD_STACK_OK on success, QD_STACK_ERR_UNDERFLOW if stack is empty
 */
qd_stack_error qd_stack_pop(qd_stack* stack, qd_stack_element_t* element);

/**
 * @brief Get the current number of elements on the stack
 *
 * @param stack Target stack
 * @return Number of elements currently on the stack
 */
size_t qd_stack_size(const qd_stack* stack);

/**
 * @brief Get the maximum capacity of the stack
 *
 * @param stack Target stack
 * @return Maximum number of elements the stack can hold
 */
size_t qd_stack_capacity(const qd_stack* stack);

/**
 * @brief Check if the stack is empty
 *
 * @param stack Target stack
 * @return true if the stack contains no elements, false otherwise
 */
bool qd_stack_is_empty(const qd_stack* stack);

/**
 * @brief Check if the stack is full
 *
 * @param stack Target stack
 * @return true if the stack is at capacity, false otherwise
 */
bool qd_stack_is_full(const qd_stack* stack);

/**
 * @defgroup ErrorTaint Error Taint Operations
 * @brief Functions for error propagation tracking
 *
 * Error tainting is used to mark values that resulted from error conditions,
 * allowing error propagation through computations in Quadrate programs.
 * @{
 */

/**
 * @brief Check if the top element is error-tainted
 *
 * @param stack Target stack
 * @return true if the top element is tainted, false otherwise
 */
bool qd_stack_is_top_tainted(const qd_stack* stack);

/**
 * @brief Mark the top element as error-tainted
 *
 * @param stack Target stack
 */
void qd_stack_mark_top_tainted(qd_stack* stack);

/**
 * @brief Clear the error taint from the top element
 *
 * @param stack Target stack
 */
void qd_stack_clear_top_taint(qd_stack* stack);

/** @} */ // end of ErrorTaint group

/**
 * @brief Convert error code to human-readable string
 *
 * @param error Error code to convert
 * @return String description of the error
 */
const char* qd_stack_error_string(qd_stack_error error);

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_STACK_H
