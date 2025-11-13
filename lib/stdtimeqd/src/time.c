#define _POSIX_C_SOURCE 199309L

#include <stdqd/time.h>
#include <qdrt/stack.h>
#include <qdrt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

