#ifndef QD_RUNTIME_RUNTIME_H
#define QD_RUNTIME_RUNTIME_H

#include <runtime/context.h>
#include <runtime/defs.h>
#include <runtime/exec_result.h>

#ifdef __cplusplus
extern "C" {
#endif

qd_exec_result qd_push(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif
