#ifndef QD_QUADRATE_RUNTIME_CONTEXT_H
#define QD_QUADRATE_RUNTIME_CONTEXT_H

#include <runtime/stack.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	qd_stack* st;
} qd_context;

#ifdef __cplusplus
}
#endif

#endif
