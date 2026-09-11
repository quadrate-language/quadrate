// Internal header for runtime implementation files
// Not part of public API - shared helpers for runtime_*.c files

#ifndef QDRT_RUNTIME_INTERNAL_H
#define QDRT_RUNTIME_INTERNAL_H

#include <quadrate/rt/qd_string.h>
#include <quadrate/rt/runtime.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Dump current stack contents for debugging (called on errors)
void qdrt_dump_stack(qd_context* ctx);

// Error handling macros - consolidate repetitive error reporting

// Fatal error with stack dump and abort
/* Terminate after a fatal runtime error.
 *
 * Uses _exit(1) rather than abort(): abort() raises SIGABRT, which on Haiku
 * triggers debug_server and hangs or garbles the output. That is the same
 * reason lib/llvmgen emits _exit(1) in generated code (generator.cc), and the
 * runtime is linked into those very programs, so the hazard applies equally
 * here. By the time this is called the message, stack dump and backtrace have
 * already been printed -- only the core file is lost.
 *
 * Set QUADRATE_ABORT_ON_FATAL=1 to get abort() back for gdb post-mortem.
 */
_Noreturn void qdrt_fatal_exit(void);

_Noreturn void qdrt_fatal_raise(qd_context* ctx, const char* op, const char* fmt, ...)
#ifdef __GNUC__
		__attribute__((format(printf, 3, 4)))
#endif
		;

#define QDRT_FATAL(ctx, op, ...) qdrt_fatal_raise((ctx), (op), __VA_ARGS__)

// Stack underflow error
#define QDRT_FATAL_UNDERFLOW(ctx, op, required, have)                                                                  \
	qdrt_fatal_raise(                                                                                                  \
			(ctx), (op), "Stack underflow (required %zu elements, have %zu)", (size_t)(required), (size_t)(have))

// Check stack has minimum elements, abort if not
#define QDRT_CHECK_STACK(ctx, op, required)                                                                            \
	do {                                                                                                               \
		size_t _have = qd_stack_size((ctx)->st);                                                                       \
		if (_have < (size_t)(required)) {                                                                              \
			QDRT_FATAL_UNDERFLOW(ctx, op, required, _have);                                                            \
		}                                                                                                              \
	} while (0)

// Check if a stack type is numeric (int or float)
static inline bool qdrt_is_numeric_type(qd_stack_type type) {
	return type == QD_STACK_TYPE_INT || type == QD_STACK_TYPE_FLOAT;
}

// Validate binary numeric operation setup
// Aborts on error, returns true if validation passed
bool qdrt_validate_binary_numeric_op(qd_context* ctx, const char* op_name);

// Pop two values from stack for binary operations
int qdrt_pop_two_values(qd_context* ctx, qd_stack_element_t* a, qd_stack_element_t* b);

// Convert stack element to double
static inline double qdrt_to_double(const qd_stack_element_t* elem) {
	return (elem->type == QD_STACK_TYPE_INT) ? (double)elem->value.i : elem->value.f;
}

// Release string reference if element is a string
static inline void qdrt_release_if_string(qd_stack_element_t* elem) {
	if (elem->type == QD_STACK_TYPE_STR) {
		qd_string_release(elem->value.s);
	}
}

// Push a stack element (retains strings)
qd_stack_error qdrt_push_element(qd_stack* stack, const qd_stack_element_t* elem);

#endif // QDRT_RUNTIME_INTERNAL_H
