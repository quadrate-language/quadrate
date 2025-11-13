#ifndef QD_STDQD_MATH_H
#define QD_STDQD_MATH_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

// Trigonometric functions
qd_exec_result usr_math_sin(qd_context* ctx);
qd_exec_result usr_math_cos(qd_context* ctx);
qd_exec_result usr_math_tan(qd_context* ctx);
qd_exec_result usr_math_asin(qd_context* ctx);
qd_exec_result usr_math_acos(qd_context* ctx);
qd_exec_result usr_math_atan(qd_context* ctx);

// Power and root functions
qd_exec_result usr_math_sqrt(qd_context* ctx);
qd_exec_result usr_math_sq(qd_context* ctx);
qd_exec_result usr_math_cb(qd_context* ctx);
qd_exec_result usr_math_cbrt(qd_context* ctx);
qd_exec_result usr_math_pow(qd_context* ctx);

// Logarithmic functions
qd_exec_result usr_math_ln(qd_context* ctx);
qd_exec_result usr_math_log10(qd_context* ctx);

// Rounding functions
qd_exec_result usr_math_ceil(qd_context* ctx);
qd_exec_result usr_math_floor(qd_context* ctx);
qd_exec_result usr_math_round(qd_context* ctx);

// Utility functions
qd_exec_result usr_math_abs(qd_context* ctx);
qd_exec_result usr_math_min(qd_context* ctx);
qd_exec_result usr_math_max(qd_context* ctx);
qd_exec_result usr_math_fac(qd_context* ctx);
qd_exec_result usr_math_inv(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif
