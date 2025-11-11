#ifndef QD_STDQD_MATH_H
#define QD_STDQD_MATH_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

// Trigonometric functions
qd_exec_result qd_stdqd_sin(qd_context* ctx);
qd_exec_result qd_stdqd_cos(qd_context* ctx);
qd_exec_result qd_stdqd_tan(qd_context* ctx);
qd_exec_result qd_stdqd_asin(qd_context* ctx);
qd_exec_result qd_stdqd_acos(qd_context* ctx);
qd_exec_result qd_stdqd_atan(qd_context* ctx);

// Power and root functions
qd_exec_result qd_stdqd_sqrt(qd_context* ctx);
qd_exec_result qd_stdqd_sq(qd_context* ctx);
qd_exec_result qd_stdqd_cb(qd_context* ctx);
qd_exec_result qd_stdqd_cbrt(qd_context* ctx);
qd_exec_result qd_stdqd_pow(qd_context* ctx);

// Logarithmic functions
qd_exec_result qd_stdqd_ln(qd_context* ctx);
qd_exec_result qd_stdqd_log10(qd_context* ctx);

// Rounding functions
qd_exec_result qd_stdqd_ceil(qd_context* ctx);
qd_exec_result qd_stdqd_floor(qd_context* ctx);
qd_exec_result qd_stdqd_round(qd_context* ctx);

// Utility functions
qd_exec_result qd_stdqd_abs(qd_context* ctx);
qd_exec_result qd_stdqd_min(qd_context* ctx);
qd_exec_result qd_stdqd_max(qd_context* ctx);
qd_exec_result qd_stdqd_fac(qd_context* ctx);
qd_exec_result qd_stdqd_inv(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif
