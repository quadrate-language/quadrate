#ifndef QD_QDMEM_MEM_H
#define QD_QDMEM_MEM_H

#include <quadrate/rt/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

// Memory allocation functions
// Named with usr_ prefix for import mechanism
int usr_mem_alloc(qd_context* ctx);
int usr_mem_realloc(qd_context* ctx);
int usr_mem_alloc_aligned(qd_context* ctx);

// Byte operations
int usr_mem_set_byte(qd_context* ctx);
int usr_mem_get_byte(qd_context* ctx);

// Integer operations (64-bit)
int usr_mem_set_i64(qd_context* ctx);
int usr_mem_get_i64(qd_context* ctx);

// Float operations (64-bit)
int usr_mem_set_f64(qd_context* ctx);
int usr_mem_get_f64(qd_context* ctx);

// Pointer operations
int usr_mem_set_ptr(qd_context* ctx);
int usr_mem_get_ptr(qd_context* ctx);

// Bulk operations
int usr_mem_copy(qd_context* ctx);
int usr_mem_zero(qd_context* ctx);
int usr_mem_fill(qd_context* ctx);

// Buffer to string conversion
int usr_mem_to_string(qd_context* ctx);
int usr_mem_from_string(qd_context* ctx);

/**
 * @brief Register this module's words on a context
 *
 * For interpreted execution: the compiler resolves these words through the
 * archive, but the interpreter looks them up by name and needs them
 * registered first. Declared in mem.qd's import block; the table is
 * generated from it, so the two cannot drift.
 *
 * @return false if ctx is NULL or a registration failed
 */
bool qd_mem_register(qd_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // STDMEMQD_MEM_H
