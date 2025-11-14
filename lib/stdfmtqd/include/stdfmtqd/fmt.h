/**
 * @file fmt.h
 * @brief Formatted output for Quadrate (fmt:: module)
 *
 * Provides printf-style formatted output functionality.
 */

#ifndef STDQD_FMT_H
#define STDQD_FMT_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Formatted print function
 *
 * Prints formatted output to stdout using printf-style format specifiers.
 *
 * @par Stack Effect: ( format:s arg1 arg2 ... argN -- )
 *
 * Format string is pushed first (at bottom), then arguments are pushed on top.
 * The function pops all stack elements, extracts the format string and arguments,
 * and prints the formatted output.
 *
 * @par Supported format specifiers:
 * - %s - String
 * - %d, %i - Integer
 * - %f - Float
 * - %% - Literal % character (no argument)
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * "Hello %s! The answer is %d\n" "World" 42 fmt::printf
 * // Output: "Hello World! The answer is 42\n"
 * @endcode
 *
 * @note Format string must be at the bottom of the stack (pushed first),
 * followed by arguments in left-to-right order (last argument on top).
 */
qd_exec_result usr_fmt_printf(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // STDQD_FMT_H
