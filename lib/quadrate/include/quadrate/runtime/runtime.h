#ifndef QD_QUADRATE_RUNTIME_RUNTIME_H
#define QD_QUADRATE_RUNTIME_RUNTIME_H

#include <quadrate/runtime/context.h>
#include <quadrate/runtime/defs.h>
#include <quadrate/runtime/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

qd_exec_result qd_push_i(qd_context* ctx, int64_t value);
qd_exec_result qd_push_f(qd_context* ctx, double value);
qd_exec_result qd_push_s(qd_context* ctx, const char* value);
qd_exec_result qd_print(qd_context* ctx);
qd_exec_result qd_printv(qd_context* ctx);
qd_exec_result qd_peek(qd_context* ctx);

qd_exec_result qd_err_push(qd_context* ctx, qd_stack_error value);

#ifdef __cplusplus
}
#endif

#endif
