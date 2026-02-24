/**
 * @file ffi.h
 * @brief Convenience header for Quadrate FFI development
 *
 * Include this single header to access all types and functions needed
 * for writing C functions callable from Quadrate code.
 *
 * Example:
 * @code
 * #include <quadrate/rt/ffi.h>
 *
 * int my_function(qd_context* ctx) {
 *     qd_stack_element_t elem;
 *     qd_stack_pop(ctx->st, &elem);
 *     // ... process elem ...
 *     return (int){0};
 * }
 * @endcode
 */

#ifndef QD_QUADRATE_RUNTIME_FFI_H
#define QD_QUADRATE_RUNTIME_FFI_H

#include <quadrate/rt/context.h>
#include <quadrate/rt/exec_result.h>
#include <quadrate/rt/stack.h>

#endif /* QD_QUADRATE_RUNTIME_FFI_H */
