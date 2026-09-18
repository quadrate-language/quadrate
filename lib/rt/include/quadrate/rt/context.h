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

#include <quadrate/rt/stack.h>
#include <setjmp.h>
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
	int64_t error_code; ///< Current error code (as chosen by the program; may be 0)
	char* error_msg;	///< Error message string (NULL if no error)
	int argc;			///< Command-line argument count
	char** argv;		///< Command-line arguments
	char* program_name; ///< Name of the executing program

	/**
	 * @brief Whether an error is currently signalled (0 = no error)
	 *
	 * `error_code` alone cannot answer this: error codes are chosen by the program, and
	 * `Err` is defined as 0, so `"msg" Err panic` writes 0 into `error_code` — which is
	 * indistinguishable from "no error". `qd_panic` therefore sets this flag
	 * unconditionally, and it is what generated code tests to decide whether a fallible
	 * call failed.
	 *
	 * Imported C functions are inconsistent about setting error state (some set
	 * `error_code`, some only push the code onto the stack) and do not set this flag, so
	 * generated code treats a non-zero `error_code` as a failure too.
	 */
	int64_t has_error;

	/** @brief Call stack for error reporting and debugging */
	const char* call_stack[QD_MAX_CALL_STACK_DEPTH];
	const char* call_stack_files[QD_MAX_CALL_STACK_DEPTH]; ///< Source files
	size_t call_stack_lines[QD_MAX_CALL_STACK_DEPTH];	   ///< Line numbers
	size_t call_stack_depth;							   ///< Current depth of the call stack

	/** @brief User-defined error context (prepended to error_msg on failure) */
	char* error_context;

	void* userdata; ///< User-defined data pointer for embedders

	// Recovery state. On the context rather than file scope: lib/qd may link
	// the shared runtime while lib/interp links the static one, and statics
	// would then exist twice in one process. Appended so codegen's GEP of
	// field 0 is unaffected.
	jmp_buf recovery_buf; ///< Armed by qd_recovery_arm()
	bool recovery_armed;  ///< Whether a fatal error unwinds instead of exiting

	/**
	 * @brief Count of assertion failures since the last reset (0 = none)
	 *
	 * An assertion reports failure by returning a non-zero exec result, and the generated
	 * code for a `test` block checks that result. It can only check the calls it emits
	 * itself, though: one frame deeper -- inside a helper, or inside `testing`'s own
	 * `assert_gt`, which is Quadrate wrapping the C `assert_true` -- the return value has
	 * nowhere to go, so the failure printed its message and the test still passed.
	 *
	 * A flag on the context does not care how deep the call was. It lives here rather than
	 * at file scope in libtesting for the same reason the recovery state does: lib/qd may
	 * link the shared runtime while lib/interp links the static one, and a static would then
	 * exist twice in one process. Appended so codegen's GEP of field 0 is unaffected.
	 */
	int64_t assertion_failures;

	struct qd_native_registry* natives; ///< Native functions, shared by both execution tiers
} qd_context;

#ifdef __cplusplus
}
#endif

#endif // QD_QUADRATE_RUNTIME_CONTEXT_H
