#ifndef QD_QUADRATE_RUNTIME_RUNTIME_H
#define QD_QUADRATE_RUNTIME_RUNTIME_H

#include <quadrate/runtime/context.h>
#include <quadrate/runtime/defs.h>
#include <quadrate/runtime/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

qd_exec_result qd_pop(qd_context* ctx);
qd_exec_result qd_push(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif
