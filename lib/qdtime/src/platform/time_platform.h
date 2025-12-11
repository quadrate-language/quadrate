#ifndef QD_QDTIME_TIME_PLATFORM_H
#define QD_QDTIME_TIME_PLATFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Get current Unix timestamp in seconds
// Returns seconds since Unix epoch (1970-01-01 00:00:00 UTC)
int64_t time_platform_unix(void);

// Get current time in nanoseconds since epoch
// Returns nanoseconds since Unix epoch
int64_t time_platform_now_ns(void);

// Sleep for specified nanoseconds
// nanoseconds: Duration to sleep (must be >= 0)
void time_platform_sleep_ns(int64_t nanoseconds);

#ifdef __cplusplus
}
#endif

#endif // QD_QDTIME_TIME_PLATFORM_H
