/**
 * @file time.h
 * @brief Time and sleep functions for Quadrate (time:: module)
 *
 * Provides time-related operations including sleep functions.
 */

#ifndef STDQD_TIME_H
#define STDQD_TIME_H

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
qd_exec_result usr_time_unix(qd_context* ctx);

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
qd_exec_result usr_time_now(qd_context* ctx);

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
qd_exec_result usr_time_sleep(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // STDQD_TIME_H
