/**
 * @file time.h
 * @brief Time and sleep functions for Quadrate (time:: module)
 *
 * Provides time-related operations including sleep functions.
 */

#ifndef QD_QDTIME_TIME_H
#define QD_QDTIME_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get current Unix timestamp in seconds
 * @return Seconds since Unix epoch (January 1, 1970 00:00:00 UTC)
 */
int64_t qd_time_unix(void);

/**
 * @brief Get current time in nanoseconds since epoch
 * @return Nanoseconds since Unix epoch
 */
int64_t qd_time_now(void);

/**
 * @brief Sleep for a specified duration in nanoseconds
 * @param nanoseconds Duration to sleep
 */
void qd_time_sleep(int64_t nanoseconds);

#ifdef __cplusplus
}
#endif

#endif // QD_QDTIME_TIME_H
