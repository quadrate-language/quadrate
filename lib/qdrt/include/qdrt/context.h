#ifndef QD_QUADRATE_RUNTIME_CONTEXT_H
#define QD_QUADRATE_RUNTIME_CONTEXT_H

#include <qdrt/stack.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QD_MAX_CALL_STACK_DEPTH 256

typedef struct {
	qd_stack* st;
	int64_t error_code;
	char* error_msg;
	int argc;
	char** argv;
	char* program_name;

	// Call stack for error reporting
	const char* call_stack[QD_MAX_CALL_STACK_DEPTH];
	size_t call_stack_depth;
} qd_context;

#ifdef __cplusplus
}
#endif

#endif
