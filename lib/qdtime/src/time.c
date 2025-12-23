#include <qdtime/time.h>
#include <stdint.h>
#include "platform/time_platform.h"

// Direct-call versions for register-based codegen
int64_t qd_time_unix(void) {
	return time_platform_unix();
}

int64_t qd_time_now(void) {
	return time_platform_now_ns();
}

void qd_time_sleep(int64_t nanoseconds) {
	if (nanoseconds > 0) {
		time_platform_sleep_ns(nanoseconds);
	}
}
