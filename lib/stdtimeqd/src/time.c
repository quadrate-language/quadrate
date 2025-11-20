#define _POSIX_C_SOURCE 199309L

#include <stdtimeqd/time.h>
#include <qdrt/stack.h>
#include <qdrt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// unix - get current Unix timestamp in seconds ( -- timestamp:i64 )
qd_exec_result usr_time_unix(qd_context* ctx) {
	time_t now = time(NULL);
	int64_t timestamp = (int64_t)now;

	qd_stack_error err = qd_stack_push_int(ctx->st, timestamp);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in time::unix: Stack overflow\n");
		abort();
	}

	return (qd_exec_result){0};
}

// now - get current time in nanoseconds since epoch ( -- nanoseconds:i64 )
qd_exec_result usr_time_now(qd_context* ctx) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	int64_t nanoseconds = (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;

	qd_stack_error err = qd_stack_push_int(ctx->st, nanoseconds);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in time::now: Stack overflow\n");
		abort();
	}

	return (qd_exec_result){0};
}

// sleep - sleep for N nanoseconds ( nanoseconds:i -- )
qd_exec_result usr_time_sleep(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in time::sleep: Stack underflow\n");
		abort();
	}

	if (val.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in time::sleep: Expected integer, got type %d\n", val.type);
		abort();
	}

	if (val.value.i < 0) {
		fprintf(stderr, "Fatal error in time::sleep: Duration cannot be negative\n");
		abort();
	}

	struct timespec ts;
	ts.tv_sec = val.value.i / 1000000000;
	ts.tv_nsec = val.value.i % 1000000000;

	nanosleep(&ts, NULL);

	return (qd_exec_result){0};
}

