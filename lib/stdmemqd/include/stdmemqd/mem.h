#ifndef STDMEMQD_MEM_H
#define STDMEMQD_MEM_H

#include <qdrt/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

// Memory allocation functions
// Named with usr_ prefix for import mechanism
qd_exec_result usr_mem_alloc(qd_context* ctx);
qd_exec_result usr_mem_free(qd_context* ctx);
qd_exec_result usr_mem_realloc(qd_context* ctx);

// Byte operations
qd_exec_result usr_mem_set_byte(qd_context* ctx);
qd_exec_result usr_mem_get_byte(qd_context* ctx);

// Word operations (64-bit)
qd_exec_result usr_mem_set(qd_context* ctx);
qd_exec_result usr_mem_get(qd_context* ctx);

// Float operations
qd_exec_result usr_mem_set_float(qd_context* ctx);
qd_exec_result usr_mem_get_float(qd_context* ctx);

// Pointer operations
qd_exec_result usr_mem_set_ptr(qd_context* ctx);
qd_exec_result usr_mem_get_ptr(qd_context* ctx);

// Bulk operations
qd_exec_result usr_mem_copy(qd_context* ctx);
qd_exec_result usr_mem_zero(qd_context* ctx);
qd_exec_result usr_mem_fill(qd_context* ctx);

// Buffer to string conversion
qd_exec_result usr_mem_to_string(qd_context* ctx);
qd_exec_result usr_mem_from_string(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // STDMEMQD_MEM_H
