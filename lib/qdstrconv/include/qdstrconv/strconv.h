/**
 * @file strconv.h
 * @brief String conversion functions for Quadrate (strconv:: module)
 *
 * Provides conversions between strings and basic types, similar to Go's strconv package.
 */

#ifndef QD_QDSTRCONV_STRCONV_H
#define QD_QDSTRCONV_STRCONV_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Format integer in given base
 * @par Stack Effect: ( value:i base:i -- str:s )
 * @param ctx Execution context
 * @return Execution result
 *
 * Converts an integer to its string representation in the specified base (2-36).
 * Examples:
 *   103 8  -> "147"  (octal)
 *   103 16 -> "67"   (hex)
 *   103 2  -> "1100111" (binary)
 */
qd_exec_result usr_strconv_format_int(qd_context* ctx);

/**
 * @brief Parse integer from string in given base
 * @par Stack Effect: ( str:s base:i -- value:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Parses a string as an integer in the specified base (2-36).
 * Aborts on invalid input.
 * Examples:
 *   "147" 8  -> 103
 *   "67" 16  -> 103
 *   "1100111" 2 -> 103
 */
qd_exec_result usr_strconv_parse_int(qd_context* ctx);

/**
 * @brief Convert integer to string (base 10)
 * @par Stack Effect: ( value:i -- str:s )
 * @param ctx Execution context
 * @return Execution result
 *
 * Equivalent to format_int with base 10.
 */
qd_exec_result usr_strconv_itoa(qd_context* ctx);

/**
 * @brief Convert string to integer (base 10)
 * @par Stack Effect: ( str:s -- value:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Equivalent to parse_int with base 10.
 */
qd_exec_result usr_strconv_atoi(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_QDSTRCONV_STRCONV_H
