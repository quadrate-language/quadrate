/**
 * @file str.h
 * @brief String manipulation functions for Quadrate (str:: module)
 *
 * Provides string operations including length, concatenation, search, and case conversion.
 */

#ifndef QD_STDQD_STR_H
#define QD_STDQD_STR_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get string length
 * @par Stack Effect: ( s:s -- len:i )
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_str_len(qd_context* ctx);

/**
 * @brief Concatenate two strings
 * @par Stack Effect: ( s1:s s2:s -- result:s )
 * @param ctx Execution context
 * @return Execution result
 *
 * Creates a new string by concatenating s1 and s2.
 */
qd_exec_result usr_str_concat(qd_context* ctx);

/**
 * @brief Check if string contains substring
 * @par Stack Effect: ( haystack:s needle:s -- bool:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Returns 1 if haystack contains needle, 0 otherwise.
 */
qd_exec_result usr_str_contains(qd_context* ctx);

/**
 * @brief Check if string starts with prefix
 * @par Stack Effect: ( s:s prefix:s -- bool:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Returns 1 if s starts with prefix, 0 otherwise.
 */
qd_exec_result usr_str_starts_with(qd_context* ctx);

/**
 * @brief Check if string ends with suffix
 * @par Stack Effect: ( s:s suffix:s -- bool:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Returns 1 if s ends with suffix, 0 otherwise.
 */
qd_exec_result usr_str_ends_with(qd_context* ctx);

/**
 * @brief Convert string to uppercase
 * @par Stack Effect: ( s:s -- upper:s )
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_str_upper(qd_context* ctx);

/**
 * @brief Convert string to lowercase
 * @par Stack Effect: ( s:s -- lower:s )
 * @param ctx Execution context
 * @return Execution result
 */
qd_exec_result usr_str_lower(qd_context* ctx);

/**
 * @brief Trim whitespace from both ends
 * @par Stack Effect: ( s:s -- trimmed:s )
 * @param ctx Execution context
 * @return Execution result
 *
 * Removes leading and trailing whitespace characters.
 */
qd_exec_result usr_str_trim(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_STDQD_STR_H
