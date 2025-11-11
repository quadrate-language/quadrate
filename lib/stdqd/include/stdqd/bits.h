#ifndef QD_STDQD_BITS_H
#define QD_STDQD_BITS_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

// Bitwise operations
qd_exec_result qd_stdqd_and(qd_context* ctx);
qd_exec_result qd_stdqd_or(qd_context* ctx);
qd_exec_result qd_stdqd_xor(qd_context* ctx);
qd_exec_result qd_stdqd_not(qd_context* ctx);
qd_exec_result qd_stdqd_lshift(qd_context* ctx);
qd_exec_result qd_stdqd_rshift(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif
