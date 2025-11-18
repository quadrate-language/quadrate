/**
 * @file context.h
 * @brief Execution context for Quadrate runtime
 *
 * Provides the execution context structure that holds the runtime state
 * for a Quadrate program, including the data stack, error state, and
 * call stack for debugging.
 */

#ifndef QD_QUADRATE_RUNTIME_CONTEXT_H
#define QD_QUADRATE_RUNTIME_CONTEXT_H

#include <qdrt/stack.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum depth of the call stack for error reporting
 */
#define QD_MAX_CALL_STACK_DEPTH 256

/**
 * @brief Execution context for a Quadrate program
 *
 * The context contains all runtime state needed to execute a Quadrate program,
 * including:
 * - The data stack for stack-based operations
 * - Error state (code and message)
 * - Command-line arguments
 * - Call stack for debugging and error reporting
 *
 * @par Typical Usage:
 * @code
 * qd_context* ctx = qd_create_context(1024);
 * // ... execute Quadrate code ...
 * qd_free_context(ctx);
 * @endcode
 */
typedef struct {
	qd_stack* st;		///< Data stack for stack-based operations
	int64_t error_code; ///< Current error code (0 = no error)
	char* error_msg;	///< Error message string (NULL if no error)
	int argc;			///< Command-line argument count
	char** argv;		///< Command-line arguments
	char* program_name; ///< Name of the executing program

	/** @brief Call stack for error reporting and debugging */
	const char* call_stack[QD_MAX_CALL_STACK_DEPTH];
	size_t call_stack_depth; ///< Current depth of the call stack
} qd_context;

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_CONTEXT_H
