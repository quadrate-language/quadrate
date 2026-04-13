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

#include <quadrate/rt/array.h>
#include <quadrate/rt/context.h>
#include <quadrate/rt/exec_result.h>
#include <quadrate/rt/qd_struct.h>

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
int qd_push_i(qd_context* ctx, int64_t value);

/**
 * @brief Push a double-precision float onto the stack
 *
 * @param ctx Execution context
 * @param value Float value to push
 * @return Execution result (0 on success)
 */
int qd_push_f(qd_context* ctx, double value);

/**
 * @brief Push a string onto the stack
 *
 * @param ctx Execution context
 * @param value String to push (will be copied)
 * @return Execution result (0 on success)
 *
 * @note The string is copied; caller retains ownership of the original
 */
int qd_push_s(qd_context* ctx, const char* value);

/**
 * @brief Push a reference-counted string onto the stack
 *
 * @param ctx Execution context
 * @param value Reference-counted string to push (will be retained)
 * @return Execution result (0 on success)
 *
 * @note The string reference count is incremented; string is shared, not copied
 */
int qd_push_s_ref(qd_context* ctx, qd_string_t* value);

/**
 * @brief Push a pointer onto the stack
 *
 * @param ctx Execution context
 * @param value Pointer value to push
 * @return Execution result (0 on success)
 *
 * @note The pointer is stored as-is; no ownership transfer occurs
 */
int qd_push_p(qd_context* ctx, void* value);

/** @} */ // end of StackPush group

/**
 * @defgroup StackPop Stack Pop Operations
 * @brief Functions for popping values from the stack
 * @{
 */

/**
 * @brief Pop a 64-bit integer from the stack
 *
 * @param ctx Execution context
 * @param[out] value Receives the integer value
 * @return 0 on success, non-zero on error (underflow or type mismatch)
 */
int qd_pop_i(qd_context* ctx, int64_t* value);

/**
 * @brief Pop a double-precision float from the stack
 *
 * @param ctx Execution context
 * @param[out] value Receives the float value
 * @return 0 on success, non-zero on error (underflow or type mismatch)
 */
int qd_pop_f(qd_context* ctx, double* value);

/**
 * @brief Pop a string from the stack
 *
 * The returned string is copied into the provided buffer. The reference-counted
 * string on the stack is released automatically.
 *
 * @param ctx Execution context
 * @param[out] buf Buffer to receive the string
 * @param buf_size Size of the buffer
 * @return 0 on success, non-zero on error (underflow, type mismatch, or truncation)
 *
 * @note The string is null-terminated. If the buffer is too small, the string
 *       is truncated and the function returns QD_ERR_GENERIC.
 */
int qd_pop_s(qd_context* ctx, char* buf, size_t buf_size);

/**
 * @brief Pop a pointer from the stack
 *
 * @param ctx Execution context
 * @param[out] value Receives the pointer value
 * @return 0 on success, non-zero on error (underflow or type mismatch)
 */
int qd_pop_p(qd_context* ctx, void** value);

/** @} */ // end of StackPop group

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
int qd_print(qd_context* ctx);

/**
 * @brief Print top stack value with verbose format
 *
 * Prints the top value with type information for debugging.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_printv(qd_context* ctx);

/**
 * @brief Print all stack values
 *
 * Prints all values currently on the stack.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_prints(qd_context* ctx);

/**
 * @brief Print all stack values with verbose format
 *
 * Prints all stack values with type information for debugging.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_printsv(qd_context* ctx);

/**
 * @brief Print a newline
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_nl(qd_context* ctx);

/**
 * @brief Read input (implementation-specific)
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_read(qd_context* ctx);

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
int qd_peek(qd_context* ctx);

/**
 * @brief Duplicate top element ( a -- a a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_dup(qd_context* ctx);

/**
 * @brief Duplicate second element ( a b -- a b a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_dupd(qd_context* ctx);

/**
 * @brief Duplicate top two elements ( a b -- a b a b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_dup2(qd_context* ctx);

/**
 * @brief Swap top two elements ( a b -- b a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_swap(qd_context* ctx);

/**
 * @brief Swap second pair of elements ( a b c -- b a c )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_swapd(qd_context* ctx);

/**
 * @brief Swap two pairs ( a b c d -- c d a b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_swap2(qd_context* ctx);

/**
 * @brief Copy second element to top ( a b -- a b a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_over(qd_context* ctx);

/**
 * @brief Copy second element below top ( a b c -- a b c a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_overd(qd_context* ctx);

/**
 * @brief Copy second pair to top ( a b c d -- a b c d a b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_over2(qd_context* ctx);

/**
 * @brief Remove second element ( a b -- b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_nip(qd_context* ctx);

/**
 * @brief Remove second element below top ( a b c -- a c )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_nipd(qd_context* ctx);

/**
 * @brief Drop top element ( a -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_drop(qd_context* ctx);

/**
 * @brief Drop top two elements ( a b -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_drop2(qd_context* ctx);

/**
 * @brief Free memory pointed to by pointer on stack ( ptr -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_free(qd_context* ctx);

/**
 * @brief Release a reference-counted struct from the stack ( ptr -- )
 *
 * Pops a struct pointer and calls qd_struct_release() on it.
 * The struct will be freed when its refcount reaches 0.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_free_struct(qd_context* ctx);

/**
 * @brief Rotate three elements ( a b c -- b c a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_rot(qd_context* ctx);

/**
 * @brief Tuck: copy top below second ( a b -- b a b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_tuck(qd_context* ctx);

/**
 * @brief Pick: copy nth element to top ( ... n -- ... n x )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_pick(qd_context* ctx);

/**
 * @brief Roll: move nth element to top ( ... n -- ... x )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_roll(qd_context* ctx);

/**
 * @brief Push stack depth ( -- n )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_depth(qd_context* ctx);

/**
 * @brief Clear the entire stack
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_clear(qd_context* ctx);

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
int qd_add(qd_context* ctx);

/**
 * @brief Subtract top from second ( a b -- a-b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_sub(qd_context* ctx);

/**
 * @brief Multiply top two elements ( a b -- a*b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_mul(qd_context* ctx);

/**
 * @brief Divide second by top ( a b -- a/b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_div(qd_context* ctx);

/**
 * @brief Modulo: remainder of division ( a b -- a%b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_mod(qd_context* ctx);

/**
 * @brief Negate top element ( a -- -a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_neg(qd_context* ctx);

/**
 * @brief Increment top element ( a -- a+1 )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_inc(qd_context* ctx);

/**
 * @brief Decrement top element ( a -- a-1 )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_dec(qd_context* ctx);

/** @} */ // end of Arithmetic group

/**
 * @defgroup Bitwise Bitwise Operations
 * @brief Functions for bitwise operations on integers
 * @{
 */

/**
 * @brief Bitwise AND ( a b -- a&b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_and(qd_context* ctx);

/**
 * @brief Bitwise OR ( a b -- a|b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_or(qd_context* ctx);

/**
 * @brief Bitwise XOR ( a b -- a^b )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_xor(qd_context* ctx);

/**
 * @brief Bitwise NOT ( a -- ~a )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_not(qd_context* ctx);

/**
 * @brief Shift left ( a n -- a<<n )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_shl(qd_context* ctx);

/**
 * @brief Shift right logical ( a n -- a>>n )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_shr(qd_context* ctx);

/** @} */ // end of Bitwise group

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
int qd_eq(qd_context* ctx);

/**
 * @brief Test inequality ( a b -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_neq(qd_context* ctx);

/**
 * @brief Test less than ( a b -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_lt(qd_context* ctx);

/**
 * @brief Test greater than ( a b -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_gt(qd_context* ctx);

/**
 * @brief Test less than or equal ( a b -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_lte(qd_context* ctx);

/**
 * @brief Test greater than or equal ( a b -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_gte(qd_context* ctx);

/**
 * @brief Test if value is within range ( val min max -- bool )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_within(qd_context* ctx);

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
int qd_casti(qd_context* ctx);

/**
 * @brief Cast top element to float
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_castf(qd_context* ctx);

/**
 * @brief Cast top element to string
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_casts(qd_context* ctx);

/**
 * @brief Cast top of stack to pointer
 *
 * Stack effect: ( value -- ptr )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_castp(qd_context* ctx);

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
int qd_spawn(qd_context* ctx);

/**
 * @brief Detach a thread
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_detach(qd_context* ctx);

/**
 * @brief Wait for a thread to complete
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_wait(qd_context* ctx);

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
int qd_call(qd_context* ctx);

/**
 * @brief Get the current error code
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_err(qd_context* ctx);

/**
 * @brief Trigger a panic (error in fallible function)
 *
 * @param ctx Execution context
 * @return Execution result (non-zero indicating error)
 */
int qd_panic(qd_context* ctx);

/**
 * @brief Get the current error code from the context
 *
 * @param ctx Execution context
 * @return Error code (0 = no error)
 */
int64_t qd_error_code(const qd_context* ctx);

/**
 * @brief Get the current error message from the context
 *
 * @param ctx Execution context
 * @return Error message string, or NULL if no error
 *
 * @note The returned pointer is valid until the next error or qd_clear_error()
 */
const char* qd_error_message(const qd_context* ctx);

/**
 * @brief Clear the error state on the context
 *
 * Resets error_code to 0 and frees error_msg.
 *
 * @param ctx Execution context
 */
void qd_clear_error(qd_context* ctx);

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

/**
 * @brief Set user-defined data pointer on context
 *
 * @param ctx Execution context
 * @param userdata Pointer to user data (embedder owns lifetime)
 */
void qd_set_userdata(qd_context* ctx, void* userdata);

/**
 * @brief Get user-defined data pointer from context
 *
 * @param ctx Execution context
 * @return User data pointer, or NULL if not set
 */
void* qd_get_userdata(qd_context* ctx);

/**
 * @brief Get the number of elements on the context's stack
 *
 * @param ctx Execution context
 * @return Number of elements currently on the stack
 */
size_t qd_context_stack_size(const qd_context* ctx);

/** @} */ // end of ContextManagement group

/**
 * @defgroup CallStack Call Stack Management
 * @brief Functions for call stack tracking (debugging/error reporting)
 * @{
 */

/**
 * @brief Push a function call onto the call stack
 *
 * Used for tracking function calls for error reporting and debugging.
 *
 * @param ctx Execution context
 * @param func_name Function name to push (must be a string literal or static string)
 * @param file Source file path (must be a string literal or static string)
 * @param line Line number in the source file
 *
 * @note The string pointers are stored directly; they must remain valid
 */
void qd_push_call(qd_context* ctx, const char* func_name, const char* file, size_t line);

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
 * @brief Register a user-defined function for coverage tracking.
 *
 * Called once per user function at the start of a coverage-instrumented
 * test runner, before any tests execute. The runtime keeps a list of
 * registered names; qd_coverage_mark marks one as called.
 *
 * @param name Function name (e.g. "module::func"). Must outlive the run.
 * @return Index of the registered function (use with qd_coverage_mark).
 */
int qd_coverage_register(const char* name);

/**
 * @brief Mark a registered function as called.
 *
 * Emitted at the start of each user function body in coverage mode.
 *
 * @param idx Index returned by qd_coverage_register.
 */
void qd_coverage_mark(int idx);

/**
 * @brief Print the coverage report (called/total + uncalled list).
 *
 * @param use_color Non-zero to use ANSI color codes in the report.
 */
void qd_coverage_report(int use_color);

/**
 * @brief Print a formatted error message for a failed function
 *
 * Prints "Fatal error: function 'name' failed: <error_msg>" to stderr.
 * If ctx->error_msg is empty, omits the ": <error_msg>" part.
 *
 * @param ctx Execution context
 * @param func_name Name of the function that failed
 */
void qd_print_error_msg(qd_context* ctx, const char* func_name);

/**
 * @brief Set user-defined error context
 *
 * Sets a context string that will be prepended to the error message
 * when a failable function fails. Cleared automatically after use.
 *
 * @param ctx Execution context
 * @param context Context string (e.g., "reading config file")
 */
void qd_set_error_context(qd_context* ctx, const char* context);

/**
 * @brief Clear user-defined error context
 *
 * @param ctx Execution context
 */
void qd_clear_error_context(qd_context* ctx);

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

/**
 * @brief Dump stack contents for error reporting
 *
 * Prints all stack values to stderr with type information.
 * Used internally for fatal error reporting.
 *
 * @param ctx Execution context
 */
void qd_dump_stack(qd_context* ctx);

/**
 * @brief Safely set error message on context
 *
 * Sets ctx->error_msg, freeing any previous message.
 * Handles strdup failure gracefully (error_msg will be NULL).
 *
 * @param ctx Execution context
 * @param msg Error message to set (will be copied)
 */
void qd_set_error_msg(qd_context* ctx, const char* msg);

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
int qd_mem_alloc(qd_context* ctx);

/**
 * @brief Free memory
 *
 * Stack: ( address:p -- )
 * Passing NULL is safe (no-op).
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_mem_free(qd_context* ctx);

/**
 * @brief Reallocate memory
 *
 * Stack: ( address:p new_bytes:i -- new_address:p )
 * Returns NULL on failure (original preserved).
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_mem_realloc(qd_context* ctx);

/**
 * @brief Set byte at address
 *
 * Stack: ( address:p offset:i value:i -- )
 * Stores lower 8 bits of value.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
int qd_mem_set_byte(qd_context* ctx);

/**
 * @brief Get byte from address
 *
 * Stack: ( address:p offset:i -- value:i )
 * Returns zero-extended byte value.
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
int qd_mem_get_byte(qd_context* ctx);

/**
 * @brief Set 64-bit integer at address
 *
 * Stack: ( address:p offset:i value:i -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
int qd_mem_set(qd_context* ctx);

/**
 * @brief Get 64-bit integer from address
 *
 * Stack: ( address:p offset:i -- value:i )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
int qd_mem_get(qd_context* ctx);

/**
 * @brief Set float at address
 *
 * Stack: ( address:p offset:i value:f -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
int qd_mem_set_float(qd_context* ctx);

/**
 * @brief Get float from address
 *
 * Stack: ( address:p offset:i -- value:f )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
int qd_mem_get_float(qd_context* ctx);

/**
 * @brief Set pointer at address
 *
 * Stack: ( address:p offset:i value:p -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
int qd_mem_set_ptr(qd_context* ctx);

/**
 * @brief Get pointer from address
 *
 * Stack: ( address:p offset:i -- value:p )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
int qd_mem_get_ptr(qd_context* ctx);

/**
 * @brief Copy memory
 *
 * Stack: ( src:p dst:p bytes:i -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
int qd_mem_copy(qd_context* ctx);

/**
 * @brief Zero memory
 *
 * Stack: ( address:p bytes:i -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
int qd_mem_zero(qd_context* ctx);

/**
 * @brief Fill memory with byte value
 *
 * Stack: ( address:p bytes:i value:i -- )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success, -1 on null pointer)
 */
int qd_mem_fill(qd_context* ctx);

/** @} */ // end of Memory group

/**
 * @defgroup PtrManagement Generic Pointer Management
 * @brief Functions for managing reference-counted pointers (arrays and structs)
 * @{
 */

/**
 * @brief Retain a generic pointer (array or struct)
 *
 * Increments the reference count of the pointer. Safe to call on any pointer -
 * will check if it's an array or struct and call the appropriate retain function.
 *
 * @param ptr Pointer to retain (can be array, struct, or other)
 * @return The same pointer
 */
void* qd_ptr_retain(void* ptr);

/**
 * @brief Release a generic pointer (array or struct)
 *
 * Decrements the reference count of the pointer. If the count reaches 0,
 * the memory is freed. Safe to call on any pointer - will check if it's
 * an array or struct and call the appropriate release function.
 *
 * @param ptr Pointer to release (can be array, struct, or other)
 */
void qd_ptr_release(void* ptr);

/** @} */ // end of PtrManagement group

/**
 * @defgroup ClosureRegistry Closure Registry
 * @brief Functions for tracking closure pointers for safe cleanup
 * @{
 */

/**
 * @brief Register a closure pointer in the closure registry
 *
 * Called when a closure is created. This allows safe detection of closures
 * without reading from potentially freed memory.
 *
 * @param ptr Closure pointer to register
 */
void qd_closure_register(void* ptr);

/**
 * @brief Check if a pointer is a registered closure
 *
 * @param ptr Pointer to check
 * @return 1 if the pointer is a registered closure, 0 otherwise
 */
int qd_closure_is_valid(const void* ptr);

/**
 * @brief Unregister a closure pointer from the registry
 *
 * Called when a closure is freed.
 *
 * @param ptr Closure pointer to unregister
 */
void qd_closure_unregister(void* ptr);

/** @} */ // end of ClosureRegistry group

/**
 * @defgroup Version Version Information
 * @brief Functions for querying runtime version
 * @{
 */

/**
 * @brief Get the Quadrate version string
 *
 * @return Version string (e.g., "2.0.0-alpha")
 */
const char* qd_version(void);

/**
 * @brief Get the Quadrate API version number
 *
 * Encoded as: major * 10000 + minor * 100 + patch
 * For example, version 2.0.0 returns 20000.
 *
 * @return API version number
 */
int qd_version_api(void);

/**
 * @brief Get the Quadrate major version number
 *
 * @return Major version number
 */
int qd_version_major(void);

/**
 * @brief Get the Quadrate minor version number
 *
 * @return Minor version number
 */
int qd_version_minor(void);

/**
 * @brief Get the Quadrate patch version number
 *
 * @return Patch version number
 */
int qd_version_patch(void);

/**
 * @brief Push version string onto stack
 *
 * Stack: ( -- version:str )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_rt_version(qd_context* ctx);

/**
 * @brief Push API version number onto stack
 *
 * Stack: ( -- api_version:i )
 *
 * @param ctx Execution context
 * @return Execution result (0 on success)
 */
int qd_rt_version_api(qd_context* ctx);

/** @} */ // end of Version group

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_RUNTIME_H
