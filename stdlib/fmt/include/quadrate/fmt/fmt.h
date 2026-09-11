/**
 * @file fmt.h
 * @brief Formatted output for Quadrate (fmt:: module)
 *
 * Provides printf-style formatted output functionality.
 */

#ifndef QD_QDFMT_FMT_H
#define QD_QDFMT_FMT_H

#include <quadrate/rt/context.h>
#include <quadrate/rt/exec_result.h>

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
int usr_fmt_printf(qd_context* ctx);

/**
 * @brief Formatted string function
 *
 * Formats a string using printf-style format specifiers and pushes the result onto the stack.
 *
 * @par Stack Effect: ( arg1 arg2 ... argN format:s -- result:s )
 *
 * Format string is on top, arguments are below it.
 * The function pops all stack elements, formats the string, and pushes the result.
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
 * "World" 42 "Hello %s! The answer is %d\n" fmt::sprintf
 * // Stack: "Hello World! The answer is 42\n"
 * @endcode
 *
 * @note Format string must be on top of the stack (pushed last),
 * with arguments below it in left-to-right order (first argument deepest).
 */
int usr_fmt_sprintf(qd_context* ctx);

/** Print a string to stdout (no newline). */
int usr_fmt_print(qd_context* ctx);

/** Print a string to stdout followed by a newline. */
int usr_fmt_println(qd_context* ctx);

/** Print a string to stderr followed by a newline. */
int usr_fmt_eprintln(qd_context* ctx);

/** Append a newline to a string and return the result. */
int usr_fmt_sprintln(qd_context* ctx);

/** Formatted print to a file descriptor. */
int usr_fmt_fprintf(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // STDQD_FMT_H
