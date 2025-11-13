#ifndef QD_STDQD_BITS_H
#define QD_STDQD_BITS_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

// Bitwise operations
qd_exec_result usr_bits_and(qd_context* ctx);
qd_exec_result usr_bits_or(qd_context* ctx);
qd_exec_result usr_bits_xor(qd_context* ctx);
qd_exec_result usr_bits_not(qd_context* ctx);
qd_exec_result usr_bits_lshift(qd_context* ctx);
qd_exec_result usr_bits_rshift(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif
