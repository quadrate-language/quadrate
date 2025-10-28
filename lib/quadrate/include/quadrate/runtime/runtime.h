#ifndef QD_QUADRATE_RUNTIME_RUNTIME_H
#define QD_QUADRATE_RUNTIME_RUNTIME_H

#include <quadrate/runtime/context.h>
#include <quadrate/runtime/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

qd_exec_result qd_push_i(qd_context* ctx, int64_t value);
qd_exec_result qd_push_f(qd_context* ctx, double value);
qd_exec_result qd_push_s(qd_context* ctx, const char* value);
qd_exec_result qd_print(qd_context* ctx);
qd_exec_result qd_printv(qd_context* ctx);
qd_exec_result qd_prints(qd_context* ctx);
qd_exec_result qd_printsv(qd_context* ctx);
qd_exec_result qd_peek(qd_context* ctx);
qd_exec_result qd_div(qd_context* ctx);
qd_exec_result qd_mul(qd_context* ctx);
qd_exec_result qd_add(qd_context* ctx);
qd_exec_result qd_sub(qd_context* ctx);
qd_exec_result qd_sq(qd_context* ctx);
qd_exec_result qd_abs(qd_context* ctx);
qd_exec_result qd_dup(qd_context* ctx);
qd_exec_result qd_swap(qd_context* ctx);

qd_exec_result qd_err_push(qd_context* ctx, qd_stack_error value);

// Check stack size and types
// types array should have 'count' elements, each specifying the expected type
// Pass QD_STACK_TYPE_PTR for untyped parameters (will skip type check)
void qd_check_stack(qd_context* ctx, size_t count, const qd_stack_type* types, const char* func_name);

#ifdef __cplusplus
}
#endif

#endif
