#ifndef QD_CGEN_STACK_H
#define QD_CGEN_STACK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
typedef enum {
	QD_STACK_OK = 0,
	QD_STACK_ERR_INVALID_CAPACITY = 1,
	QD_STACK_ERR_OVERFLOW = 2,
	QD_STACK_ERR_UNDERFLOW = 3,
	QD_STACK_ERR_TYPE_MISMATCH = 4,
	QD_STACK_ERR_NULL_POINTER = 5,
	QD_STACK_ERR_ALLOC = 6
} qd_stack_error_t;

/* Element types */
typedef enum {
	QD_STACK_TYPE_INT,
	QD_STACK_TYPE_DOUBLE,
	QD_STACK_TYPE_PTR,
	QD_STACK_TYPE_STR
} qd_stack_type_t;

/* Stack structure */
typedef struct qd_stack qd_stack_t;

/* Lifecycle functions */
qd_stack_error_t qd_stack_init(qd_stack_t** stack, size_t capacity);
void qd_stack_destroy(qd_stack_t* stack);

/* Stack operations */
qd_stack_error_t qd_stack_push_int(qd_stack_t* stack, int64_t value);
qd_stack_error_t qd_stack_push_double(qd_stack_t* stack, double value);
qd_stack_error_t qd_stack_push_ptr(qd_stack_t* stack, void* value);
qd_stack_error_t qd_stack_push_str(qd_stack_t* stack, const char* value);

qd_stack_error_t qd_stack_pop(qd_stack_t* stack);

qd_stack_error_t qd_stack_top_type(const qd_stack_t* stack, qd_stack_type_t* type);
qd_stack_error_t qd_stack_top_int(const qd_stack_t* stack, int64_t* value);
qd_stack_error_t qd_stack_top_double(const qd_stack_t* stack, double* value);
qd_stack_error_t qd_stack_top_ptr(const qd_stack_t* stack, void** value);
qd_stack_error_t qd_stack_top_str(const qd_stack_t* stack, const char** value);

/* Introspection functions */
size_t qd_stack_size(const qd_stack_t* stack);
size_t qd_stack_capacity(const qd_stack_t* stack);
bool qd_stack_is_empty(const qd_stack_t* stack);
bool qd_stack_is_full(const qd_stack_t* stack);

/* Error message helper */
const char* qd_stack_error_string(qd_stack_error_t error);

#ifdef __cplusplus
}
#endif

#endif
