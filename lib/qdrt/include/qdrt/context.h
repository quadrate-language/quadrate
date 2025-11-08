#ifndef QD_QUADRATE_RUNTIME_CONTEXT_H
#define QD_QUADRATE_RUNTIME_CONTEXT_H

#include <qdrt/stack.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	qd_stack* st;
	bool has_error;
	int argc;
	char** argv;
} qd_context;

#ifdef __cplusplus
}
#endif

#endif
