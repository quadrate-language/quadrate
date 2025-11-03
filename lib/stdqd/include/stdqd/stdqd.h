#ifndef STDQD_H
#define STDQD_H

#include <quadrate/runtime/context.h>
#include <quadrate/runtime/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * libstdqd - Quadrate Standard Library
 *
 * This library provides standard library functions for Quadrate programs,
 * including networking, file I/O, and other common utilities.
 *
 * All functions follow the convention:
 * - Take qd_context* as first parameter
 * - Return qd_exec_result
 * - Manipulate the stack for input/output
 */

// Version information
#define STDQD_VERSION_MAJOR 0
#define STDQD_VERSION_MINOR 1
#define STDQD_VERSION_PATCH 0

/**
 * Get version string
 * Returns: "0.1.0"
 */
const char* stdqd_version(void);

// ============================================================================
// Standard I/O Functions
// ============================================================================

/**
 * qd_stdqd_printf - Formatted print function
 *
 * Stack signature: ( arg1 arg2 ... argN format:s -- )
 *
 * Pops a format string from the stack, then pops the required number of
 * arguments based on format specifiers, and prints the formatted output.
 *
 * Supported format specifiers:
 * - %s - String (pops string from stack)
 * - %d, %i - Integer (pops int from stack)
 * - %f - Float (pops float from stack)
 * - %% - Literal % character (no argument)
 *
 * Example in Quadrate:
 *   "World" 42 "Hello %s! The answer is %d\n" qd_stdqd_printf
 *   Output: "Hello World! The answer is 42\n"
 *
 * Note: Arguments are pushed to stack in left-to-right order, but format
 * string is on top, so they're popped in reverse order during formatting.
 */
qd_exec_result qd_stdqd_printf(qd_context* ctx);

// Networking functions will be added here
// File I/O functions will be added here

#ifdef __cplusplus
}
#endif

#endif // STDQD_H
