#ifndef QD_QUADRATE_RUNTIME_RUNTIME_H
#define QD_QUADRATE_RUNTIME_RUNTIME_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

qd_exec_result qd_push_i(qd_context* ctx, int64_t value);
qd_exec_result qd_push_f(qd_context* ctx, double value);
qd_exec_result qd_push_s(qd_context* ctx, const char* value);
qd_exec_result qd_push_p(qd_context* ctx, void* value);
qd_exec_result qd_print(qd_context* ctx);
qd_exec_result qd_printv(qd_context* ctx);
qd_exec_result qd_prints(qd_context* ctx);
qd_exec_result qd_printsv(qd_context* ctx);
qd_exec_result qd_nl(qd_context* ctx);
qd_exec_result qd_peek(qd_context* ctx);
qd_exec_result qd_div(qd_context* ctx);
qd_exec_result qd_mul(qd_context* ctx);
qd_exec_result qd_add(qd_context* ctx);
qd_exec_result qd_sub(qd_context* ctx);
qd_exec_result qd_dup(qd_context* ctx);
qd_exec_result qd_dupd(qd_context* ctx);
qd_exec_result qd_dup2(qd_context* ctx);
qd_exec_result qd_swapd(qd_context* ctx);
qd_exec_result qd_overd(qd_context* ctx);
qd_exec_result qd_nipd(qd_context* ctx);
qd_exec_result qd_swap(qd_context* ctx);
qd_exec_result qd_over(qd_context* ctx);
qd_exec_result qd_nip(qd_context* ctx);
qd_exec_result qd_call(qd_context* ctx);
qd_exec_result qd_dec(qd_context* ctx);
qd_exec_result qd_inc(qd_context* ctx);
qd_exec_result qd_casti(qd_context* ctx);
qd_exec_result qd_castf(qd_context* ctx);
qd_exec_result qd_casts(qd_context* ctx);
qd_exec_result qd_clear(qd_context* ctx);
qd_exec_result qd_depth(qd_context* ctx);
qd_exec_result qd_eq(qd_context* ctx);
qd_exec_result qd_neq(qd_context* ctx);
qd_exec_result qd_lt(qd_context* ctx);
qd_exec_result qd_gt(qd_context* ctx);
qd_exec_result qd_lte(qd_context* ctx);
qd_exec_result qd_gte(qd_context* ctx);
qd_exec_result qd_within(qd_context* ctx);
qd_exec_result qd_drop(qd_context* ctx);
qd_exec_result qd_drop2(qd_context* ctx);
qd_exec_result qd_rot(qd_context* ctx);
qd_exec_result qd_tuck(qd_context* ctx);
qd_exec_result qd_pick(qd_context* ctx);
qd_exec_result qd_roll(qd_context* ctx);
qd_exec_result qd_swap2(qd_context* ctx);
qd_exec_result qd_over2(qd_context* ctx);
qd_exec_result qd_mod(qd_context* ctx);
qd_exec_result qd_neg(qd_context* ctx);
qd_exec_result qd_spawn(qd_context* ctx);
qd_exec_result qd_detach(qd_context* ctx);
qd_exec_result qd_wait(qd_context* ctx);
qd_exec_result qd_err(qd_context* ctx);
qd_exec_result qd_error(qd_context* ctx);
qd_exec_result qd_read(qd_context* ctx);

qd_exec_result qd_err_push(qd_context* ctx, qd_stack_error value);

// Check stack size and types
// types array should have 'count' elements, each specifying the expected type
// Pass QD_STACK_TYPE_PTR for untyped parameters (will skip type check)
void qd_check_stack(qd_context* ctx, size_t count, const qd_stack_type* types, const char* func_name);

// Context management
qd_context* qd_create_context(size_t stack_size);
void qd_free_context(qd_context* ctx);

// Call stack management for debugging/error reporting
void qd_push_call(qd_context* ctx, const char* func_name);
void qd_pop_call(qd_context* ctx);
void qd_print_stack_trace(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif
