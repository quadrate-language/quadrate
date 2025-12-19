// Internal header for runtime implementation files
// Not part of public API - shared helpers for runtime_*.c files

#ifndef QDRT_RUNTIME_INTERNAL_H
#define QDRT_RUNTIME_INTERNAL_H

#include <qdrt/qd_string.h>
#include <qdrt/runtime.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Dump current stack contents for debugging (called on errors)
void qdrt_dump_stack(qd_context* ctx);

// Check if a stack type is numeric (int or float)
static inline bool qdrt_is_numeric_type(qd_stack_type type) {
	return type == QD_STACK_TYPE_INT || type == QD_STACK_TYPE_FLOAT;
}

// Validate binary numeric operation setup
// Aborts on error, returns true if validation passed
bool qdrt_validate_binary_numeric_op(qd_context* ctx, const char* op_name);

// Pop two values from stack for binary operations
qd_exec_result qdrt_pop_two_values(qd_context* ctx, qd_stack_element_t* a, qd_stack_element_t* b);

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
