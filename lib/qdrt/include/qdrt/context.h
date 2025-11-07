#ifndef QD_QUADRATE_RUNTIME_CONTEXT_H
#define QD_QUADRATE_RUNTIME_CONTEXT_H

#include <stdbool.h>
#include <qdrt/stack.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	qd_stack* st;
	bool has_error;
} qd_context;

#ifdef __cplusplus
}
#endif

#endif
