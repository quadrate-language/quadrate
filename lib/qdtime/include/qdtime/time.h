/**
 * @file time.h
 * @brief Time and sleep functions for Quadrate (time:: module)
 *
 * Provides time-related operations including sleep functions.
 */

#ifndef QD_QDTIME_TIME_H
#define QD_QDTIME_TIME_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get current Unix timestamp in seconds
 *
 * @par Stack Effect: ( -- timestamp:i64 )
 *
 * Returns the number of seconds since Unix epoch (January 1, 1970 00:00:00 UTC).
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_time_unix(qd_context* ctx);

/**
 * @brief Get current time in nanoseconds since epoch
 *
 * @par Stack Effect: ( -- nanoseconds:i64 )
 *
 * Returns high-precision time as nanoseconds since Unix epoch.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_time_now(qd_context* ctx);

/**
 * @brief Sleep for a specified duration in nanoseconds
 *
 * @par Stack Effect: ( nanoseconds:i -- )
 *
 * Suspends execution for the specified duration in nanoseconds.
 *
 * @param ctx Execution context
 * @return Execution result
 *
 * @par Example:
 * @code
 * time::Second time::sleep  // Sleep for 1 second
 * 500 time::Millisecond mul time::sleep  // Sleep for 500 milliseconds
 * time::Millisecond time::sleep  // Sleep for 1 millisecond
 * @endcode
 *
 * @note Time constants (Second, Millisecond, etc.) are defined in the time module
 */
int usr_time_sleep(qd_context* ctx);

/**
 * @brief Format a Unix timestamp using strftime format string
 *
 * @par Stack Effect: ( timestamp:i format:s -- result:s )!
 *
 * Formats the timestamp according to the format string.
 * Common format specifiers:
 * - %Y: 4-digit year
 * - %m: Month (01-12)
 * - %d: Day (01-31)
 * - %H: Hour (00-23)
 * - %M: Minute (00-59)
 * - %S: Second (00-59)
 * - %F: Date (YYYY-MM-DD)
 * - %T: Time (HH:MM:SS)
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_time_format(qd_context* ctx);

/**
 * @brief Parse a string into a Unix timestamp using strptime format string
 *
 * @par Stack Effect: ( str:s format:s -- timestamp:i )!
 *
 * Parses the string according to the format and returns Unix timestamp.
 *
 * @param ctx Execution context
 * @return Execution result
 */
int usr_time_parse(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // STDQD_TIME_H
