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
 *     int64_t val;
 *     qd_pop_i(ctx, &val);
 *     // ... process val ...
 *     return qd_push_i(ctx, val * 2);
 * }
 * @endcode
 */

#ifndef QD_QUADRATE_RUNTIME_FFI_H
#define QD_QUADRATE_RUNTIME_FFI_H

#include <quadrate/rt/runtime.h>

#endif /* QD_QUADRATE_RUNTIME_FFI_H */
