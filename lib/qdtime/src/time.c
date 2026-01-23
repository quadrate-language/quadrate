#include <qdtime/time.h>
#include <qdrt/stack.h>
#include <qdrt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include "platform/time_platform.h"

// unix - get current Unix timestamp in seconds ( -- timestamp:i64 )
int usr_time_unix(qd_context* ctx) {
	int64_t timestamp = time_platform_unix();

	qd_stack_error err = qd_stack_push_int(ctx->st, timestamp);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in time::unix: Stack overflow\n");
		abort();
	}

	return (int){0};
}

// now - get current time in nanoseconds since epoch ( -- nanoseconds:i64 )
int usr_time_now(qd_context* ctx) {
	int64_t nanoseconds = time_platform_now_ns();

	qd_stack_error err = qd_stack_push_int(ctx->st, nanoseconds);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in time::now: Stack overflow\n");
		abort();
	}

	return (int){0};
}

// sleep - sleep for N nanoseconds ( nanoseconds:i -- )
int usr_time_sleep(qd_context* ctx) {
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

	time_platform_sleep_ns(val.value.i);

	return (int){0};
}
