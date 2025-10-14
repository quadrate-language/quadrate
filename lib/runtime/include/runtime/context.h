#ifndef QD_RUNTIME_CONTEXT_H
#define QD_RUNTIME_CONTEXT_H

#include <runtime/stack.h>

#ifndef __cplusplus
extern "C" {
#endif

typedef struct {
	qd_stack st;
} qd_context;

#ifndef __cplusplus
}
#endif

#endif
