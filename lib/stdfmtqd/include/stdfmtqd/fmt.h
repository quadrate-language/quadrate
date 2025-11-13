#ifndef STDQD_FMT_H
#define STDQD_FMT_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Standard I/O Functions
// ============================================================================

/**
 * usr_fmt_printf - Formatted print function
 *
 * Stack signature: ( format:s arg1 arg2 ... argN -- )
 *
 * Format string is pushed first (at bottom), then arguments are pushed on top.
 * The function pops all stack elements, extracts the format string and arguments,
 * and prints the formatted output.
 *
 * Supported format specifiers:
 * - %s - String
 * - %d, %i - Integer
 * - %f - Float
 * - %% - Literal % character (no argument)
 *
 * Example in Quadrate:
 *   "Hello %s! The answer is %d\n" "World" 42 fmt::printf
 *   Output: "Hello World! The answer is 42\n"
 *
 * Note: Format string must be at the bottom of the stack (pushed first),
 * followed by arguments in left-to-right order (last argument on top).
 */
qd_exec_result usr_fmt_printf(qd_context* ctx);

// Networking functions will be added here
// File I/O functions will be added here

#ifdef __cplusplus
}
#endif

#endif // STDQD_H
