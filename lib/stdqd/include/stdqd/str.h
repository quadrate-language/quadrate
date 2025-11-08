#ifndef QD_STDQD_STR_H
#define QD_STDQD_STR_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

// String length
qd_exec_result qd_stdqd_len(qd_context* ctx);

// String concatenation
qd_exec_result qd_stdqd_concat(qd_context* ctx);

// String search
qd_exec_result qd_stdqd_contains(qd_context* ctx);
qd_exec_result qd_stdqd_starts_with(qd_context* ctx);
qd_exec_result qd_stdqd_ends_with(qd_context* ctx);

// Case conversion
qd_exec_result qd_stdqd_upper(qd_context* ctx);
qd_exec_result qd_stdqd_lower(qd_context* ctx);

// Whitespace trimming
qd_exec_result qd_stdqd_trim(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif
