#define _POSIX_C_SOURCE 199309L

#include <stdqd/time.h>
#include <qdrt/stack.h>
#include <qdrt/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// sleep - sleep for N nanoseconds ( nanoseconds:i -- )
qd_exec_result qd_stdqd_sleep(qd_context* ctx) {
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

qd_exec_result qd_stdqd_Nanosecond(qd_context* ctx) {
	qd_push_i(ctx, 1);
	return (qd_exec_result){0};
}

qd_exec_result qd_stdqd_Microsecond(qd_context* ctx) {
	qd_push_i(ctx, 1000);
	return (qd_exec_result){0};
}

qd_exec_result qd_stdqd_Millisecond(qd_context* ctx) {
	qd_push_i(ctx, 1000000);
	return (qd_exec_result){0};
}

qd_exec_result qd_stdqd_Second(qd_context* ctx) {
	qd_push_i(ctx, 1000000000);
	return (qd_exec_result){0};
}

qd_exec_result qd_stdqd_Minute(qd_context* ctx) {
	qd_push_i(ctx, 60000000000);
	return (qd_exec_result){0};
}

qd_exec_result qd_stdqd_Hour(qd_context* ctx) {
	qd_push_i(ctx, 3600000000000);
	return (qd_exec_result){0};
}
