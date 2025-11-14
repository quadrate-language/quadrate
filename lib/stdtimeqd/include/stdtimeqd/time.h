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
