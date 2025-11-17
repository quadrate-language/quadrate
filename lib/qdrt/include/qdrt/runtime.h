/**
 * @file runtime.h
 * @brief Core runtime functions for Quadrate execution
 *
 * Provides low-level runtime functions for stack-based execution of Quadrate programs.
 * This includes:
 * - Stack manipulation (push, pop, dup, swap, etc.)
 * - Arithmetic operations (add, sub, mul, div, etc.)
 * - I/O operations (print, read)
 * - Context management (create, destroy)
 * - Error handling and call stack tracking
 */

#ifndef QD_QUADRATE_RUNTIME_RUNTIME_H
#define QD_QUADRATE_RUNTIME_RUNTIME_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup StackPush Stack Push Operations
 * @brief Functions for pushing values onto the stack
 * @{
 */

/**
 * @brief Push a 64-bit integer onto the stack
 *
 * @param ctx Execution context
 * @param value Integer value to push
 * @return Execution result (0 on success)
 */
qd_exec_result qd_push_i(qd_context* ctx, int64_t value);

/**
 * @brief Push a double-precision float onto the stack
 *
 * @param ctx Execution context
 * @param value Float value to push
 * @return Execution result (0 on success)
 */
qd_exec_result qd_push_f(qd_context* ctx, double value);

/**
 * @brief Push a string onto the stack
 *
 * @param ctx Execution context
 * @param value String to push (will be copied)
 * @return Execution result (0 on success)
 *
 * @note The string is copied; caller retains ownership of the original
 */
qd_exec_result qd_push_s(qd_context* ctx, const char* value);

/**
 * @brief Push a pointer onto the stack
 *
 * @param ctx Execution context
 * @param value Pointer value to push
 * @return Execution result (0 on success)
 *
 * @note The pointer is stored as-is; no ownership transfer occurs
 */
qd_exec_result qd_push_p(qd_context* ctx, void* value);

/** @} */ // end of StackPush group

/**
 * @defgroup IO Input/Output Operations
 * @brief Functions for I/O operations
 * @{
 */

/**
 * @brief Print top stack value without newline
 *
 * Prints the top value from the stack to stdout without adding a newline.
 * The value remains on the stack.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_print(qd_context* ctx);

/**
 * @brief Print top stack value with verbose format
 *
 * Prints the top value with type information for debugging.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_printv(qd_context* ctx);

/**
 * @brief Print all stack values
 *
 * Prints all values currently on the stack.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_prints(qd_context* ctx);

/**
 * @brief Print all stack values with verbose format
 *
 * Prints all stack values with type information for debugging.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_printsv(qd_context* ctx);

/**
 * @brief Print a newline
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_nl(qd_context* ctx);

/**
 * @brief Read input (implementation-specific)
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_read(qd_context* ctx);

/** @} */ // end of IO group

/**
 * @defgroup StackManip Stack Manipulation Operations
 * @brief Functions for manipulating the stack
 * @{
 */

/**
 * @brief Peek at the top stack element
 *
 * Examines the top element without removing it.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_peek(qd_context* ctx);

/**
 * @brief Duplicate top element ( a -- a a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_dup(qd_context* ctx);

/**
 * @brief Duplicate second element ( a b -- a b a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_dupd(qd_context* ctx);

/**
 * @brief Duplicate top two elements ( a b -- a b a b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_dup2(qd_context* ctx);

/**
 * @brief Swap top two elements ( a b -- b a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_swap(qd_context* ctx);

/**
 * @brief Swap second pair of elements ( a b c -- b a c )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_swapd(qd_context* ctx);

/**
 * @brief Swap two pairs ( a b c d -- c d a b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_swap2(qd_context* ctx);

/**
 * @brief Copy second element to top ( a b -- a b a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_over(qd_context* ctx);

/**
 * @brief Copy second element below top ( a b c -- a b c a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_overd(qd_context* ctx);

/**
 * @brief Copy second pair to top ( a b c d -- a b c d a b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_over2(qd_context* ctx);

/**
 * @brief Remove second element ( a b -- b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_nip(qd_context* ctx);

/**
 * @brief Remove second element below top ( a b c -- a c )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_nipd(qd_context* ctx);

/**
 * @brief Drop top element ( a -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_drop(qd_context* ctx);

/**
 * @brief Drop top two elements ( a b -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_drop2(qd_context* ctx);

/**
 * @brief Rotate three elements ( a b c -- b c a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_rot(qd_context* ctx);

/**
 * @brief Tuck: copy top below second ( a b -- b a b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_tuck(qd_context* ctx);

/**
 * @brief Pick: copy nth element to top ( ... n -- ... n x )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_pick(qd_context* ctx);

/**
 * @brief Roll: move nth element to top ( ... n -- ... x )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_roll(qd_context* ctx);

/**
 * @brief Push stack depth ( -- n )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_depth(qd_context* ctx);

/**
 * @brief Clear the entire stack
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_clear(qd_context* ctx);

/** @} */ // end of StackManip group

/**
 * @defgroup Arithmetic Arithmetic Operations
 * @brief Functions for arithmetic operations
 * @{
 */

/**
 * @brief Add top two elements ( a b -- a+b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_add(qd_context* ctx);

/**
 * @brief Subtract top from second ( a b -- a-b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_sub(qd_context* ctx);

/**
 * @brief Multiply top two elements ( a b -- a*b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_mul(qd_context* ctx);

/**
 * @brief Divide second by top ( a b -- a/b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_div(qd_context* ctx);

/**
 * @brief Modulo: remainder of division ( a b -- a%b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_mod(qd_context* ctx);

/**
 * @brief Negate top element ( a -- -a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_neg(qd_context* ctx);

/**
 * @brief Increment top element ( a -- a+1 )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_inc(qd_context* ctx);

/**
 * @brief Decrement top element ( a -- a-1 )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_dec(qd_context* ctx);

/** @} */ // end of Arithmetic group

/**
 * @defgroup Comparison Comparison Operations
 * @brief Functions for comparison operations
 * @{
 */

/**
 * @brief Test equality ( a b -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_eq(qd_context* ctx);

/**
 * @brief Test inequality ( a b -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_neq(qd_context* ctx);

/**
 * @brief Test less than ( a b -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_lt(qd_context* ctx);

/**
 * @brief Test greater than ( a b -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_gt(qd_context* ctx);

/**
 * @brief Test less than or equal ( a b -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_lte(qd_context* ctx);

/**
 * @brief Test greater than or equal ( a b -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_gte(qd_context* ctx);

/**
 * @brief Test if value is within range ( val min max -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_within(qd_context* ctx);

/** @} */ // end of Comparison group

/**
 * @defgroup TypeCast Type Casting Operations
 * @brief Functions for type conversion
 * @{
 */

/**
 * @brief Cast top element to integer
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_casti(qd_context* ctx);

/**
 * @brief Cast top element to float
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_castf(qd_context* ctx);

/**
 * @brief Cast top element to string
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_casts(qd_context* ctx);

/** @} */ // end of TypeCast group

/**
 * @defgroup Threading Threading Operations
 * @brief Functions for concurrency
 * @{
 */

/**
 * @brief Spawn a new thread
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_spawn(qd_context* ctx);

/**
 * @brief Detach a thread
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_detach(qd_context* ctx);

/**
 * @brief Wait for a thread to complete
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_wait(qd_context* ctx);

/** @} */ // end of Threading group

/**
 * @defgroup ErrorHandling Error Handling
 * @brief Functions for error management
 * @{
 */

/**
 * @brief Call a function pointer from the stack
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_call(qd_context* ctx);

/**
 * @brief Get the current error code
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_err(qd_context* ctx);

/**
 * @brief Trigger an error
 *
 * @param ctx Execution context
 * @return Execution result (non-zero indicating error)
 */
qd_exec_result qd_error(qd_context* ctx);

/**
 * @brief Push an error code onto the stack
 *
 * @param ctx Execution context
 * @param value Error code to push
 * @return Execution result (0 on success)
 */
qd_exec_result qd_err_push(qd_context* ctx, qd_stack_error value);

/** @} */ // end of ErrorHandling group

/**
 * @defgroup StackValidation Stack Validation
 * @brief Functions for runtime stack validation
 * @{
 */

/**
 * @brief Check stack size and types
 *
 * Validates that the stack has at least 'count' elements and that
 * each element matches the expected type. This is used internally
 * for runtime type checking.
 *
 * @param ctx Execution context
 * @param count Number of elements to check
 * @param types Array of expected types (length must be >= count)
 * @param func_name Function name for error messages
 *
 * @note Pass QD_STACK_TYPE_PTR to skip type checking for a parameter
 * @note If validation fails, sets error state in context
 */
void qd_check_stack(qd_context* ctx, size_t count, const qd_stack_type* types, const char* func_name);

/** @} */ // end of StackValidation group

/**
 * @defgroup ContextManagement Context Management
 * @brief Functions for managing execution contexts
 * @{
 */

/**
 * @brief Create a new execution context
 *
 * Allocates and initializes a new execution context with the specified
 * stack size.
 *
 * @param stack_size Maximum number of elements the stack can hold
 * @return Pointer to the new context, or NULL on allocation failure
 *
 * @note The caller is responsible for freeing the context with qd_free_context()
 */
qd_context* qd_create_context(size_t stack_size);

/**
 * @brief Free an execution context
 *
 * Frees all resources associated with the context, including the stack.
 *
 * @param ctx Context to free (can be NULL)
 */
void qd_free_context(qd_context* ctx);

/**
 * @brief Clone an execution context (deep copy)
 *
 * Creates a deep copy of the source context, including the entire stack.
 * This is used by the ctx keyword to create an isolated execution context.
 *
 * @param src Source context to clone
 * @return Pointer to the cloned context, or NULL on failure
 *
 * @note The caller is responsible for freeing the cloned context with qd_free_context()
 * @note The stack and all string values are deep copied
 * @note Command-line arguments and program name are shared (not copied)
 */
qd_context* qd_clone_context(const qd_context* src);

/** @} */ // end of ContextManagement group

/**
 * @defgroup CallStack Call Stack Management
 * @brief Functions for call stack tracking (debugging/error reporting)
 * @{
 */

/**
 * @brief Push a function name onto the call stack
 *
 * Used for tracking function calls for error reporting and debugging.
 *
 * @param ctx Execution context
 * @param func_name Function name to push (must be a string literal or static string)
 *
 * @note The function name pointer is stored directly; it must remain valid
 */
void qd_push_call(qd_context* ctx, const char* func_name);

/**
 * @brief Pop a function name from the call stack
 *
 * @param ctx Execution context
 */
void qd_pop_call(qd_context* ctx);

/**
 * @brief Print the current call stack trace
 *
 * Prints the call stack to stderr for debugging purposes.
 *
 * @param ctx Execution context
 */
void qd_print_stack_trace(qd_context* ctx);

/**
 * @brief Print the data stack contents for debugging
 *
 * Prints all values currently on the data stack to stderr.
 *
 * Usage in GDB:
 *   call (void)qd_debug_print_stack(ctx)
 *
 * Note: The (void) cast is required because GDB cannot determine the
 * return type from the debug symbols.
 *
 * @param ctx Execution context
 */
void qd_debug_print_stack(qd_context* ctx);

/** @} */ // end of CallStack group

/**
 * @defgroup Memory Memory Management Operations
 * @brief Functions for dynamic memory allocation and access
 * @{
 */

/**
 * @brief Allocate memory
 *
 * Stack: ( bytes:i -- address:p )
 * Returns NULL (0) on failure.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_mem_alloc(qd_context* ctx);

/**
 * @brief Free memory
 *
 * Stack: ( address:p -- )
 * Passing NULL is safe (no-op).
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_mem_free(qd_context* ctx);

/**
 * @brief Reallocate memory
 *
 * Stack: ( address:p new_bytes:i -- new_address:p )
 * Returns NULL on failure (original preserved).
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
qd_exec_result qd_mem_realloc(qd_context* ctx);

/**
 * @brief Set byte at address
 *
 * Stack: ( address:p offset:i value:i -- )
 * Stores lower 8 bits of value.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
qd_exec_result qd_mem_set_byte(qd_context* ctx);

/**
 * @brief Get byte from address
 *
 * Stack: ( address:p offset:i -- value:i )
 * Returns zero-extended byte value.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
qd_exec_result qd_mem_get_byte(qd_context* ctx);

/**
 * @brief Set 64-bit integer at address
 *
 * Stack: ( address:p offset:i value:i -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
qd_exec_result qd_mem_set(qd_context* ctx);

/**
 * @brief Get 64-bit integer from address
 *
 * Stack: ( address:p offset:i -- value:i )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
qd_exec_result qd_mem_get(qd_context* ctx);

/**
 * @brief Set float at address
 *
 * Stack: ( address:p offset:i value:f -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
qd_exec_result qd_mem_set_float(qd_context* ctx);

/**
 * @brief Get float from address
 *
 * Stack: ( address:p offset:i -- value:f )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
qd_exec_result qd_mem_get_float(qd_context* ctx);

/**
 * @brief Set pointer at address
 *
 * Stack: ( address:p offset:i value:p -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
qd_exec_result qd_mem_set_ptr(qd_context* ctx);

/**
 * @brief Get pointer from address
 *
 * Stack: ( address:p offset:i -- value:p )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
qd_exec_result qd_mem_get_ptr(qd_context* ctx);

/**
 * @brief Copy memory
 *
 * Stack: ( src:p dst:p bytes:i -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
qd_exec_result qd_mem_copy(qd_context* ctx);

/**
 * @brief Zero memory
 *
 * Stack: ( address:p bytes:i -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
qd_exec_result qd_mem_zero(qd_context* ctx);

/**
 * @brief Fill memory with byte value
 *
 * Stack: ( address:p bytes:i value:i -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
qd_exec_result qd_mem_fill(qd_context* ctx);

/** @} */ // end of Memory group

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_RUNTIME_H
