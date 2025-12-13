/**
 * @file ffi.h
 * @brief Convenience header for Quadrate FFI development
 *
 * Include this single header to access all types and functions needed
 * for writing C functions callable from Quadrate code.
 *
 * Example:
 * @code
 * #include <qdrt/ffi.h>
 *
 * qd_exec_result my_function(qd_context* ctx) {
 *     qd_stack_element_t elem;
 *     qd_stack_pop(ctx->st, &elem);
 *     // ... process elem ...
 *     return (qd_exec_result){0};
 * }
 * @endcode
 */

#ifndef QD_QUADRATE_RUNTIME_FFI_H
#define QD_QUADRATE_RUNTIME_FFI_H

#include <qdrt/context.h>
#include <qdrt/exec_result.h>
#include <qdrt/stack.h>

#endif /* QD_QUADRATE_RUNTIME_FFI_H */
