#include <quadrate/runtime/runtime.h>

qd_exec_result qd_push_i(qd_context* ctx, int64_t value) {
	if (ctx == NULL || ctx->st == NULL) {
		return (qd_exec_result){-1};
	}
	qd_stack_error err = qd_stack_push_int(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_push_f(qd_context* ctx, double value) {
	if (ctx == NULL || ctx->st == NULL) {
		return (qd_exec_result){-1};
	}
	qd_stack_error err = qd_stack_push_double(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_push_s(qd_context* ctx, const char* value) {
	if (ctx == NULL || ctx->st == NULL || value == NULL) {
		return (qd_exec_result){-1};
	}
	qd_stack_error err = qd_stack_push_str(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_err_push(qd_context* /*ctx*/, qd_stack_error /*value*/) {
	return (qd_exec_result){0};
}
