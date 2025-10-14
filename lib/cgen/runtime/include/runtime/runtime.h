#ifndef QD_RUNTIME_RUNTIME_H
#define QD_RUNTIME_RUNTIME_H

#include <runtime/context.h>
#include <runtime/exec_result.h>

#ifndef __cplusplus
extern "C" {
#endif

qd_exec_result qd_push(qd_context* ctx);


#ifndef __cplusplus
}
#endif

#endif
