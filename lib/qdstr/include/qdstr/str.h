/**
 * @file str.h
 * @brief String manipulation functions for Quadrate (str:: module)
 *
 * Provides string operations including length, concatenation, search, and case conversion.
 */

#ifndef QD_QDSTR_STR_H
#define QD_QDSTR_STR_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

/**
 * @brief Error codes for str module
 *
 * These match the constants defined in str/module.qd
 * Ok=1 (success), specific errors start at 2
 */
#define STR_ERR_OK 1			/**< Success (matches builtin Ok) */
#define STR_ERR_OUT_OF_BOUNDS 2 /**< Index out of bounds */
#define STR_ERR_ALLOC 3			/**< Memory allocation failed */
#define STR_ERR_INVALID_ARG 4	/**< Invalid argument */

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

/**
 * @brief Extract substring
 * @par Stack Effect: ( s:s start:i length:i -- result:s )
 * @param ctx Execution context
 * @return Execution result
 *
 * Returns substring starting at index 'start' with length 'length'.
 * Aborts if indices are out of bounds.
 */
qd_exec_result usr_str_substring(qd_context* ctx);

/**
 * @brief Split string by delimiter
 * @par Stack Effect: ( s:s delim:s -- parts:p count:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Returns pointer to array of strings and count. Caller must free each string and the array.
 */
qd_exec_result usr_str_split(qd_context* ctx);

/**
 * @brief Replace all occurrences of substring
 * @par Stack Effect: ( s:s old:s new:s -- result:s )
 * @param ctx Execution context
 * @return Execution result
 *
 * Replaces all occurrences of 'old' with 'new' in string.
 */
qd_exec_result usr_str_replace(qd_context* ctx);

/**
 * @brief Compare two strings
 * @par Stack Effect: ( s1:s s2:s -- result:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Returns -1 if s1 < s2, 0 if equal, 1 if s1 > s2.
 */
qd_exec_result usr_str_compare(qd_context* ctx);

/**
 * @brief Get character code at index
 * @par Stack Effect: ( s:s index:i -- char_code:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Returns the ASCII/Unicode code point at the given index.
 * Aborts if index is out of bounds.
 */
qd_exec_result usr_str_char_at(qd_context* ctx);

/**
 * @brief Find index of substring
 * @par Stack Effect: ( haystack:s needle:s -- index:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Returns the index of the first occurrence of needle in haystack.
 * Returns -1 if not found.
 */
qd_exec_result usr_str_index_of(qd_context* ctx);

/**
 * @brief Find index of substring starting from position
 * @par Stack Effect: ( haystack:s needle:s start:i -- index:i )
 * @param ctx Execution context
 * @return Execution result
 *
 * Returns the index of the first occurrence of needle in haystack,
 * starting the search from the given position.
 * Returns -1 if not found.
 */
qd_exec_result usr_str_index_of_from(qd_context* ctx);

/**
 * @brief Create a single-character string from character code
 * @par Stack Effect: ( char_code:i -- s:s )
 * @param ctx Execution context
 * @return Execution result
 *
 * Creates a string containing a single character with the given code point.
 */
qd_exec_result usr_str_from_char(qd_context* ctx);

/**
 * @brief Sort array of strings in ascending order
 * @par Stack Effect: ( arr:p count:i -- )
 * @param ctx Execution context
 * @return Execution result
 *
 * Sorts the array of string pointers in place using strcmp comparison.
 */
qd_exec_result usr_str_sort(qd_context* ctx);

/**
 * @brief Sort array of strings in descending order
 * @par Stack Effect: ( arr:p count:i -- )
 * @param ctx Execution context
 * @return Execution result
 *
 * Sorts the array of string pointers in place using strcmp comparison (reversed).
 */
qd_exec_result usr_str_sort_desc(qd_context* ctx);

/**
 * @brief Convert C string pointer to Quadrate string
 * @par Stack Effect: ( ptr:p -- str:s )
 * @param ctx Execution context
 * @return Execution result
 *
 * Converts a pointer to a null-terminated C string into a Quadrate string.
 * If the pointer is NULL, returns an empty string.
 */
qd_exec_result usr_str_from_ptr(qd_context* ctx);

/**
 * @brief Join array of strings with delimiter
 * @par Stack Effect: ( parts:p count:i delim:s -- result:s )!
 * @param ctx Execution context
 * @return Execution result
 *
 * Joins an array of strings using the given delimiter.
 * Returns a new string with all parts concatenated.
 */
qd_exec_result usr_str_join(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // QD_QDSTR_STR_H
