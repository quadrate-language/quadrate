#ifndef QD_QUADRATE_RUNTIME_STACK_H
#define QD_QUADRATE_RUNTIME_STACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
} qd_stack_error;

/* Element types */
typedef enum {
	QD_STACK_TYPE_INT,
	QD_STACK_TYPE_FLOAT,
	QD_STACK_TYPE_PTR,
	QD_STACK_TYPE_STR
} qd_stack_type;

typedef struct {
	union {
		int64_t i;
		double f;
		void* p;
		char* s;
	} value;

	qd_stack_type type;
} qd_stack_element_t;

typedef struct qd_stack qd_stack;

/* Lifecycle functions */
qd_stack_error qd_stack_init(qd_stack** stack, size_t capacity);
void qd_stack_destroy(qd_stack* stack);

/* Stack operations */
qd_stack_error qd_stack_push_int(qd_stack* stack, int64_t value);
qd_stack_error qd_stack_push_float(qd_stack* stack, double value);
qd_stack_error qd_stack_push_ptr(qd_stack* stack, void* value);
qd_stack_error qd_stack_push_str(qd_stack* stack, const char* value);

qd_stack_error qd_stack_peek(qd_stack* stack, qd_stack_element_t* element);
qd_stack_error qd_stack_element(qd_stack* stack, size_t index, qd_stack_element_t* element);
qd_stack_error qd_stack_pop(qd_stack* stack);

qd_stack_error qd_stack_pop_type(const qd_stack* stack, qd_stack_type* type);
qd_stack_error qd_stack_pop_int(const qd_stack* stack, int64_t* value);
qd_stack_error qd_stack_pop_float(const qd_stack* stack, double* value);
qd_stack_error qd_stack_pop_ptr(const qd_stack* stack, void** value);
qd_stack_error qd_stack_pop_str(const qd_stack* stack, const char** value);

/* Introspection functions */
size_t qd_stack_size(const qd_stack* stack);
size_t qd_stack_capacity(const qd_stack* stack);
bool qd_stack_is_empty(const qd_stack* stack);
bool qd_stack_is_full(const qd_stack* stack);

/* Error message helper */
const char* qd_stack_error_string(qd_stack_error error);

#ifdef __cplusplus
}
#endif

#endif
