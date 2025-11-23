// POSIX time implementation
#define _POSIX_C_SOURCE 200809L

#include "../time_platform.h"
#include <time.h>

// Get current Unix timestamp in seconds
int64_t time_platform_unix(void) {
	return (int64_t)time(NULL);
}

// Get current time in nanoseconds since epoch
int64_t time_platform_now_ns(void) {
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

// Sleep for specified nanoseconds
void time_platform_sleep_ns(int64_t nanoseconds) {
	struct timespec ts;
	ts.tv_sec = nanoseconds / 1000000000;
	ts.tv_nsec = nanoseconds % 1000000000;
	nanosleep(&ts, NULL);
}
