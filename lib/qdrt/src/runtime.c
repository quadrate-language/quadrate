// Need POSIX for strdup() - not part of C standard
#define _POSIX_C_SOURCE 200809L

#include <qdrt/runtime.h>
#include <qdrt/array.h>
#include <qdrt/qd_string.h>
#include <qdrt/qd_struct.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "platform/thread_platform.h"
#include "ptr_registry.h"
#include "runtime_internal.h"

static void dump_stack(qd_context* ctx);

// Closure registry for safe closure detection
static ptr_registry_t closure_registry = PTR_REGISTRY_INITIALIZER;

void qd_closure_register(void* ptr) {
	if (ptr) {
		ptr_registry_add(&closure_registry, ptr);
	}
}

int qd_closure_is_valid(const void* ptr) {
	if (!ptr) {
		return 0;
	}
	return ptr_registry_contains(&closure_registry, ptr);
}

void qd_closure_unregister(void* ptr) {
	if (ptr) {
		ptr_registry_remove(&closure_registry, ptr);
	}
}

qd_exec_result qd_push_i(qd_context* ctx, int64_t value) {
	qd_stack_error err = qd_stack_push_int(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_push_f(qd_context* ctx, double value) {
	qd_stack_error err = qd_stack_push_float(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_push_s(qd_context* ctx, const char* value) {
	qd_stack_error err = qd_stack_push_str(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

qd_exec_result qd_push_s_ref(qd_context* ctx, qd_string_t* value) {
	if (value == NULL) {
		return (qd_exec_result){-2};
	}

	// Retain the string (increment reference count)
	qd_string_retain(value);

	// Check stack capacity
	if (qd_stack_size(ctx->st) >= ctx->st->capacity) {
		qd_string_release(value);  // Cleanup on overflow
		return (qd_exec_result){-2};
	}

	// Push directly to stack
	ctx->st->data[ctx->st->size].value.s = value;
	ctx->st->data[ctx->st->size].type = QD_STACK_TYPE_STR;
	ctx->st->data[ctx->st->size].is_error_tainted = false;
	ctx->st->size++;

	return (qd_exec_result){0};
}

qd_exec_result qd_push_p(qd_context* ctx, void* value) {
	qd_stack_error err = qd_stack_push_ptr(ctx->st, value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

// Helper function to check if string contains whitespace
qd_exec_result qd_peek(qd_context* ctx) {
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_peek(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	switch (val.type) {
		case QD_STACK_TYPE_INT:
			printf("%ld\n", val.value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			printf("%f\n", val.value.f);
			break;
		case QD_STACK_TYPE_STR:
			printf("%s\n", qd_string_data(val.value.s));
			break;
		default:
			return (qd_exec_result){-3};
	}
	return (qd_exec_result){0};
}

// Helper function to check if a stack type is numeric
static inline bool is_numeric_type(qd_stack_type type) {
	return type == QD_STACK_TYPE_INT || type == QD_STACK_TYPE_FLOAT;
}

// Helper function to validate binary numeric operation setup
// Returns true if validation passed, false if error occurred (and already aborted)
static bool validate_binary_numeric_op(qd_context* ctx, const char* op_name) {
	// Check we have at least 2 elements
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		QDRT_FATAL_UNDERFLOW(ctx, op_name, 2, stack_size);
	}

	// Check both are numeric types
	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		QDRT_FATAL(ctx, op_name, "Failed to access stack elements");
	}

	if (!is_numeric_type(check_a.type) || !is_numeric_type(check_b.type)) {
		QDRT_FATAL(ctx, op_name, "Type error (expected numeric types)");
	}

	return true;
}

// Helper function to pop two values from stack for binary operations
static qd_exec_result pop_two_values(qd_context* ctx, qd_stack_element_t* a, qd_stack_element_t* b) {
	qd_stack_error err = qd_stack_pop(ctx->st, b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	return (qd_exec_result){0};
}

// Helper function to release string references if needed
static void release_if_string(qd_stack_element_t* elem) {
	if (elem->type == QD_STACK_TYPE_STR) {
		qd_string_release(elem->value.s);
	}
}

// Helper function to push a stack element (retains strings)
static qd_stack_error push_element(qd_stack* stack, const qd_stack_element_t* elem) {
	qd_stack_error err;
	switch (elem->type) {
		case QD_STACK_TYPE_INT:
			err = qd_stack_push_int(stack, elem->value.i);
			break;
		case QD_STACK_TYPE_FLOAT:
			err = qd_stack_push_float(stack, elem->value.f);
			break;
		case QD_STACK_TYPE_STR:
			// Retain the string reference (increment refcount) and push
			qd_string_retain(elem->value.s);
			if (qd_stack_size(stack) >= stack->capacity) {
				qd_string_release(elem->value.s);  // Cleanup on overflow
				return QD_STACK_ERR_OVERFLOW;
			}
			stack->data[stack->size].value.s = elem->value.s;
			stack->data[stack->size].type = QD_STACK_TYPE_STR;
			stack->data[stack->size].is_error_tainted = elem->is_error_tainted;
			stack->size++;
			return QD_STACK_OK;
		case QD_STACK_TYPE_PTR:
			err = qd_stack_push_ptr(stack, elem->value.p);
			break;
		default:
			return QD_STACK_ERR_TYPE_MISMATCH;
	}
	if (err == QD_STACK_OK) {
		// Copy error taint flag for non-string types
		stack->data[stack->size - 1].is_error_tainted = elem->is_error_tainted;
	}
	return err;
}

// sqrt - square root

// cb - cube (x^3)

// cbrt - cube root

// ceil - ceiling (round up)

// floor - floor (round down)

// ln - natural logarithm (base e)

// log10 - base 10 logarithm

// Closure magic marker (0xCL05UR3E in leet speak)
#define QD_CLOSURE_MAGIC 0xC105023E

// Closure struct layout: { int64_t magic, void* fn_ptr, void* env_ptr }
typedef struct {
	int64_t magic;
	void* fn_ptr;
	void* env_ptr;
} qd_closure_t;

// call - invoke function pointer or closure from stack
qd_exec_result qd_call(qd_context* ctx) {
	// Pop function pointer/closure and call it
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "call", "Stack underflow");
	}

	// Verify it's a pointer type
	if (val.type != QD_STACK_TYPE_PTR) {
		QDRT_FATAL(ctx, "call", "Expected pointer type, got %d");
	}

	void* ptr = val.value.p;
	if (ptr == NULL) {
		QDRT_FATAL(ctx, "call", "NULL pointer");
	}

	// Check if this is a closure (magic marker at start of struct)
	qd_closure_t* closure = (qd_closure_t*)ptr;
	if (closure->magic == QD_CLOSURE_MAGIC) {
		// This is a closure - extract function pointer and environment
		// Closure function signature: qd_exec_result (*)(qd_context*, void* env)
		typedef qd_exec_result (*qd_closure_fn_ptr)(qd_context*, void*);
		qd_closure_fn_ptr func;
		memcpy(&func, &closure->fn_ptr, sizeof(func));

		if (func == NULL) {
			QDRT_FATAL(ctx, "call", "NULL closure function pointer");
		}

		// Call the closure with environment
		return func(ctx, closure->env_ptr);
	} else {
		// Regular function pointer
		// Function signature: qd_exec_result (*)(qd_context*)
		typedef qd_exec_result (*qd_function_ptr)(qd_context*);
		qd_function_ptr func;
		memcpy(&func, &ptr, sizeof(func));

		// Call the function
		return func(ctx);
	}
}

// dec - decrement (subtract 1, preserves type)
// inc - increment (add 1, preserves type)
// casti - cast top stack element to integer
qd_exec_result qd_casti(qd_context* ctx) {
	// Pop one value, convert to integer, push result
	QDRT_CHECK_STACK(ctx, "casti", 1);

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "casti", "Failed to pop value");
	}

	int64_t result;
	if (elem.type == QD_STACK_TYPE_INT) {
		result = elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		result = (int64_t)elem.value.f;
	} else if (elem.type == QD_STACK_TYPE_STR) {
		result = atoll(qd_string_data(elem.value.s));
		release_if_string(&elem);  // Release the string after conversion
	} else {
		QDRT_FATAL(ctx, "casti", "Cannot cast type to integer");
	}

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// castf - cast top stack element to float
qd_exec_result qd_castf(qd_context* ctx) {
	// Pop one value, convert to float, push result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		QDRT_FATAL(ctx, "castf", "Stack underflow (requires 1 value)");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "castf", "Failed to pop value");
	}

	double result;
	if (elem.type == QD_STACK_TYPE_INT) {
		result = (double)elem.value.i;
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		result = elem.value.f;
	} else if (elem.type == QD_STACK_TYPE_STR) {
		result = atof(qd_string_data(elem.value.s));
		release_if_string(&elem);  // Release the string after conversion
	} else {
		QDRT_FATAL(ctx, "castf", "Cannot cast type to float");
	}

	err = qd_stack_push_float(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// casts - cast top stack element to string
qd_exec_result qd_casts(qd_context* ctx) {
	// Pop one value, convert to string, push result
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		QDRT_FATAL(ctx, "casts", "Stack underflow (requires 1 value)");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "casts", "Failed to pop value");
	}

	char buffer[64];
	if (elem.type == QD_STACK_TYPE_INT) {
		snprintf(buffer, sizeof(buffer), "%ld", elem.value.i);
	} else if (elem.type == QD_STACK_TYPE_FLOAT) {
		snprintf(buffer, sizeof(buffer), "%g", elem.value.f);
	} else if (elem.type == QD_STACK_TYPE_STR) {
		// Already a string - just push it back (retain it)
		err = push_element(ctx->st, &elem);
		if (err != QD_STACK_OK) {
			return (qd_exec_result){-2};
		}
		// Release our reference from the pop
		release_if_string(&elem);
		return (qd_exec_result){0};
	} else if (elem.type == QD_STACK_TYPE_PTR) {
		// Treat pointer as qd_string_t* - retain it and push as string
		qd_string_t* str = (qd_string_t*)elem.value.p;
		if (str) {
			qd_string_retain(str);
			qd_stack_element_t str_elem = {.type = QD_STACK_TYPE_STR, .value.s = str};
			err = push_element(ctx->st, &str_elem);
			if (err != QD_STACK_OK) {
				qd_string_release(str);
				return (qd_exec_result){-2};
			}
			return (qd_exec_result){0};
		} else {
			// NULL pointer - push empty string
			err = qd_stack_push_str(ctx->st, "");
			if (err != QD_STACK_OK) {
				return (qd_exec_result){-2};
			}
			return (qd_exec_result){0};
		}
	} else {
		QDRT_FATAL(ctx, "casts", "Cannot cast type to string");
	}

	err = qd_stack_push_str(ctx->st, buffer);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// castp - cast to pointer
qd_exec_result qd_castp(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		QDRT_FATAL(ctx, "castp", "Stack underflow (requires 1 value)");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "castp", "Failed to pop value");
	}

	void* ptr_value = NULL;
	if (elem.type == QD_STACK_TYPE_PTR) {
		// Already a pointer - just push it back
		ptr_value = elem.value.p;
	} else if (elem.type == QD_STACK_TYPE_INT) {
		// Cast integer to pointer
		ptr_value = (void*)(intptr_t)elem.value.i;
	} else {
		QDRT_FATAL(ctx, "castp", "Cannot cast type %d to pointer");
	}

	err = qd_stack_push_ptr(ctx->st, ptr_value);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// pow - exponentiation (base^exponent)

// round - round to nearest integer

// clear - empty the entire stack
// depth - push the current stack depth onto the stack
// fac - factorial (n!)

// inv - inverse/reciprocal (1/x, returns float)

// eq - equal (==) comparison: ( a b -- result )
// Pops two values, pushes 1 if equal, 0 otherwise
// neq - not equal (!=) comparison: ( a b -- result )
// Pops two values, pushes 1 if not equal, 0 otherwise
// lt - less than (<) comparison: ( a b -- result )
// Pops two values, pushes 1 if a < b, 0 otherwise
// gt - greater than (>) comparison: ( a b -- result )
// Pops two values, pushes 1 if a > b, 0 otherwise
// lte - less than or equal (<=) comparison: ( a b -- result )
// Pops two values, pushes 1 if a <= b, 0 otherwise
// gte - greater than or equal (>=) comparison: ( a b -- result )
// Pops two values, pushes 1 if a >= b, 0 otherwise
// within - check if value is within range [min, max]: ( value min max -- result )
// Pops three values, pushes 1 if min <= value <= max, 0 otherwise
// Dump current stack contents for debugging
static void dump_stack(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	fprintf(stderr, "\nStack dump (%zu elements):\n", stack_size);

	if (stack_size == 0) {
		fprintf(stderr, "  (empty)");
		return;
	}

	for (size_t i = 0; i < stack_size; i++) {
		qd_stack_element_t elem;
		qd_stack_error err = qd_stack_element(ctx->st, i, &elem);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "  [%zu]: <error reading element>\n", i);
			continue;
		}

		fprintf(stderr, "  [%zu]: ", i);
		switch (elem.type) {
			case QD_STACK_TYPE_INT:
				fprintf(stderr, "int = %ld\n", elem.value.i);
				break;
			case QD_STACK_TYPE_FLOAT:
				fprintf(stderr, "float = %f\n", elem.value.f);
				break;
			case QD_STACK_TYPE_STR:
				fprintf(stderr, "str = \"%s\"\n", qd_string_data(elem.value.s));
				break;
			case QD_STACK_TYPE_PTR:
				fprintf(stderr, "ptr = %p\n", elem.value.p);
				break;
			default:
				fprintf(stderr, "<unknown type>");
				break;
		}
	}
}

void qd_check_stack(qd_context* ctx, size_t count, const qd_stack_type* types, const char* func_name) {
	qd_stack* st = ctx->st;

	// Fast path: direct struct access for common cases
	size_t stack_size = st->size;
	if (stack_size < count) {
		fprintf(stderr, "Fatal error in %s: Stack underflow (required %zu elements, have %zu)\n",
			func_name, count, stack_size);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Fast path: single integer parameter (very common case like fib(n:i64))
	if (count == 1 && types[0] == QD_STACK_TYPE_INT) {
		if (st->data[stack_size - 1].type == QD_STACK_TYPE_INT) {
			return;  // Fast success
		}
		// Fall through to error reporting
		const char* actual_type_name = "";
		switch (st->data[stack_size - 1].type) {
			case QD_STACK_TYPE_INT: actual_type_name = "int"; break;
			case QD_STACK_TYPE_FLOAT: actual_type_name = "float"; break;
			case QD_STACK_TYPE_STR: actual_type_name = "str"; break;
			case QD_STACK_TYPE_PTR: actual_type_name = "ptr"; break;
		}
		fprintf(stderr, "Fatal error in %s: Type mismatch for parameter 1 (expected int, got %s)\n",
			func_name, actual_type_name);
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Check types match (from bottom to top of required elements)
	for (size_t i = 0; i < count; i++) {
		// Skip type check for untyped parameters (marked with QD_STACK_TYPE_PTR)
		if (types[i] == QD_STACK_TYPE_PTR) {
			continue;
		}

		// Direct struct access instead of function call
		size_t stack_index = stack_size - count + i;
		qd_stack_element_t* elem = &st->data[stack_index];

		if (elem->type != types[i]) {
			const char* expected_type_name = "";
			const char* actual_type_name = "";

			switch (types[i]) {
				case QD_STACK_TYPE_INT: expected_type_name = "int"; break;
				case QD_STACK_TYPE_FLOAT: expected_type_name = "float"; break;
				case QD_STACK_TYPE_STR: expected_type_name = "str"; break;
				case QD_STACK_TYPE_PTR: expected_type_name = "ptr"; break;
			}

			switch (elem->type) {
				case QD_STACK_TYPE_INT: actual_type_name = "int"; break;
				case QD_STACK_TYPE_FLOAT: actual_type_name = "float"; break;
				case QD_STACK_TYPE_STR: actual_type_name = "str"; break;
				case QD_STACK_TYPE_PTR: actual_type_name = "ptr"; break;
			}

			fprintf(stderr, "Fatal error in %s: Type mismatch for parameter %zu (expected %s, got %s)\n",
				func_name, i + 1, expected_type_name, actual_type_name);
			dump_stack(ctx);
			qd_print_stack_trace(ctx);
			abort();
		}
	}
}

// drop - remove top element from stack: ( a -- )
// drop2 - remove top 2 elements from stack: ( a b -- )
// free - deallocate memory pointed to by pointer on stack: ( ptr -- )
qd_exec_result qd_free(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		QDRT_FATAL(ctx, "free", "Stack underflow (required 1 element, have %zu)");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Verify it's a pointer type
	if (val.type != QD_STACK_TYPE_PTR) {
		QDRT_FATAL(ctx, "free", "Expected pointer type, got type %d");
	}

	// Check if this is a struct pointer (uses registry lookup)
	if (qd_struct_is_valid(val.value.p)) {
		// Struct pointers are offset from the malloc'd base, use release
		qd_struct_release(val.value.p);
	} else if (qd_array_is_valid(val.value.p)) {
		// Array pointer - use array release
		qd_array_release((qd_array_t*)val.value.p);
	} else {
		// Raw memory - free directly (ptr can be NULL, free(NULL) is safe)
		free(val.value.p);
	}

	return (qd_exec_result){0};
}

// free_struct - release reference-counted struct from stack: ( ptr -- )
// Uses qd_struct_release which decrements refcount and frees when it reaches 0
qd_exec_result qd_free_struct(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		QDRT_FATAL(ctx, "free", "Stack underflow (required 1 element, have %zu)");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Verify it's a pointer type
	if (val.type != QD_STACK_TYPE_PTR) {
		QDRT_FATAL(ctx, "free", "Expected pointer type, got type %d");
	}

	// Release the struct (decrements refcount, frees when 0)
	qd_struct_release(val.value.p);

	return (qd_exec_result){0};
}

// rot - rotate top 3 elements: ( a b c -- b c a )
// tuck - insert copy of top below second: ( a b -- b a b )
// pick - copy nth element to top (0-indexed from top): ( ... n -- ... nth )
// roll - rotate n elements, moving nth to top: ( ... n -- ... )
// swap2 - swap top two pairs: ( a b c d -- c d a b )
// over2 - copy second pair to top: ( a b c d -- a b c d a b )
// mod - modulo operation: ( a b -- a%b )
// and - bitwise AND: ( a b -- a&b )
// or - bitwise OR: ( a b -- a|b )
// xor - bitwise XOR: ( a b -- a^b )
// not - bitwise NOT: ( a -- ~a )
// shl - shift left: ( a n -- a<<n )
// shr - shift right (logical): ( a n -- a>>n )
// neg - negate top element: ( a -- -a )
// min - minimum of top 2 elements: ( a b -- min(a,b) )

// max - maximum of top 2 elements: ( a b -- max(a,b) )

// Threading support
typedef struct {
	qd_context* ctx;
	void* func_ptr;
} qd_thread_info_t;

// Wrapper function for platform threads that calls the Quadrate function
static int qd_thread_wrapper(void* arg) {
	qd_thread_info_t* info = (qd_thread_info_t*)arg;

	// Call the function
	typedef qd_exec_result (*qd_function_ptr)(qd_context*);
	qd_function_ptr func;
	memcpy(&func, &info->func_ptr, sizeof(func));

	if (func) {
		func(info->ctx);
	}

	// Clean up context
	qd_free_context(info->ctx);
	free(info);

	return 0;
}

// spawn - create a new thread ( fn:ptr -- thread_id:i )
qd_exec_result qd_spawn(qd_context* ctx) {
	// Pop function pointer
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "spawn", "Stack underflow");
	}

	// Verify it's a pointer type
	if (val.type != QD_STACK_TYPE_PTR) {
		QDRT_FATAL(ctx, "spawn", "Expected pointer type, got %d");
	}

	// Create new context for the thread
	qd_context* thread_ctx = qd_create_context(1024);
	if (!thread_ctx) {
		QDRT_FATAL(ctx, "spawn", "Failed to create context");
		abort();
	}

	// Create thread info
	qd_thread_info_t* info = malloc(sizeof(qd_thread_info_t));
	if (!info) {
		QDRT_FATAL(ctx, "spawn", "Failed to allocate thread info");
		qd_free_context(thread_ctx);
		abort();
	}
	info->ctx = thread_ctx;
	info->func_ptr = val.value.p;

	// Create thread using platform abstraction
	thread_handle_t thread = thread_platform_create(qd_thread_wrapper, info);
	if (!thread) {
		QDRT_FATAL(ctx, "spawn", "thread_platform_create failed");
		qd_free_context(thread_ctx);
		free(info);
		abort();
	}

	// Push thread handle (as pointer cast to int64_t)
	err = qd_stack_push_int(ctx->st, (int64_t)(uintptr_t)thread);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

// detach - detach a thread ( thread_id:i -- )
qd_exec_result qd_detach(qd_context* ctx) {
	// Pop thread ID
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "detach", "Stack underflow");
	}

	// Verify it's an integer type
	if (val.type != QD_STACK_TYPE_INT) {
		QDRT_FATAL(ctx, "detach", "Expected integer type, got %d");
	}

	// Get thread handle
	thread_handle_t thread = (thread_handle_t)(uintptr_t)val.value.i;

	// Detach thread using platform abstraction
	int result = thread_platform_detach(thread);
	if (result != THREAD_SUCCESS) {
		QDRT_FATAL(ctx, "detach", "thread_platform_detach failed");
		abort();
	}

	return (qd_exec_result){0};
}

// wait - join/wait for a thread ( thread_id:i -- )
qd_exec_result qd_wait(qd_context* ctx) {
	// Pop thread ID
	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);

	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "wait", "Stack underflow");
	}

	// Verify it's an integer type
	if (val.type != QD_STACK_TYPE_INT) {
		QDRT_FATAL(ctx, "wait", "Expected integer type, got %d");
	}

	// Get thread handle
	thread_handle_t thread = (thread_handle_t)(uintptr_t)val.value.i;

	// Join thread using platform abstraction
	int result = thread_platform_join(thread);
	if (result != THREAD_SUCCESS) {
		QDRT_FATAL(ctx, "wait", "thread_platform_join failed");
		abort();
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_err(qd_context* ctx) {
	// Retrieve error information from the last failed fallible function call
	// Stack effect: ( -- msg code )
	// Push error message (empty string if no error)
	const char* msg = ctx->error_msg ? ctx->error_msg : "";
	qd_stack_error err = qd_stack_push_str(ctx->st, msg);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "err", "Failed to push error message");
	}

	// Push error code
	err = qd_stack_push_int(ctx->st, ctx->error_code);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "err", "Failed to push error code");
	}

	// Clear error state after reading
	ctx->error_code = 0;
	if (ctx->error_msg) {
		free(ctx->error_msg);
		ctx->error_msg = NULL;
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_panic(qd_context* ctx) {
	// Pop error code and message from stack and set error state
	// Stack notation: ( msg code -- ) means msg is pushed first, code is on top
	// Stack after: []
	qd_stack_element_t error_msg_elem, error_code_elem;

	// Pop error code (integer) - it's on top
	qd_stack_error err = qd_stack_pop(ctx->st, &error_code_elem);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "panic", "Stack underflow when popping error code");
	}

	if (error_code_elem.type != QD_STACK_TYPE_INT) {
		QDRT_FATAL(ctx, "panic", "Expected integer error code, got type %d");
	}

	// Pop error message (string)
	err = qd_stack_pop(ctx->st, &error_msg_elem);
	if (err != QD_STACK_OK) {
		QDRT_FATAL(ctx, "panic", "Stack underflow when popping error message");
	}

	if (error_msg_elem.type != QD_STACK_TYPE_STR) {
		QDRT_FATAL(ctx, "panic", "Expected string error message, got type %d");
		dump_stack(ctx);
		qd_print_stack_trace(ctx);
		// Release the error code's string reference if needed
		if (error_code_elem.type == QD_STACK_TYPE_STR) {
			qd_string_release(error_code_elem.value.s);
		}
		abort();
	}

	// Set error code and message
	ctx->error_code = error_code_elem.value.i;

	// Free old error message if it exists
	if (ctx->error_msg) {
		free(ctx->error_msg);
	}

	// Duplicate the error message string (context owns a copy)
	ctx->error_msg = strdup(qd_string_data(error_msg_elem.value.s));
	// strdup failure results in NULL error_msg, which is acceptable
	// (error_code still indicates the error condition)
	// Release the qd_string_t
	release_if_string(&error_msg_elem);

	return (qd_exec_result){0};
}

// Helper function to check if string is an integer
// Helper function to check if string is a float
// Helper function to remove quotes from a string
// Context management functions
qd_context* qd_create_context(size_t stack_size) {
	qd_context* ctx = (qd_context*)malloc(sizeof(qd_context));
	if (ctx) {
		qd_stack_error err = qd_stack_init(&ctx->st, stack_size);
		if (err != QD_STACK_OK) {
			free(ctx);
			return NULL;
		}
		ctx->error_code = 0;
		ctx->error_msg = NULL;
		ctx->error_context = NULL;
		ctx->argc = 0;
		ctx->argv = NULL;
		ctx->program_name = NULL;
		ctx->call_stack_depth = 0;
	}
	return ctx;
}

void qd_free_context(qd_context* ctx) {
	if (ctx == NULL) {
		return;
	}
	qd_stack_destroy(ctx->st);
	if (ctx->program_name) {
		free(ctx->program_name);
	}
	if (ctx->error_msg) {
		free(ctx->error_msg);
	}
	if (ctx->error_context) {
		free(ctx->error_context);
	}
	free(ctx);
}

qd_context* qd_clone_context(const qd_context* src) {
	if (src == NULL) {
		return NULL;
	}

	/* Allocate new context */
	qd_context* ctx = (qd_context*)malloc(sizeof(qd_context));
	if (ctx == NULL) {
		return NULL;
	}

	/* Clone the stack */
	qd_stack_error err = qd_stack_clone(&ctx->st, src->st);
	if (err != QD_STACK_OK) {
		free(ctx);
		return NULL;
	}

	/* Copy error state */
	ctx->error_code = src->error_code;
	if (src->error_msg != NULL) {
		ctx->error_msg = strdup(src->error_msg);
		if (ctx->error_msg == NULL) {
			qd_stack_destroy(ctx->st);
			free(ctx);
			return NULL;
		}
	} else {
		ctx->error_msg = NULL;
	}
	if (src->error_context != NULL) {
		ctx->error_context = strdup(src->error_context);
		if (ctx->error_context == NULL) {
			if (ctx->error_msg) free(ctx->error_msg);
			qd_stack_destroy(ctx->st);
			free(ctx);
			return NULL;
		}
	} else {
		ctx->error_context = NULL;
	}

	/* Share command-line arguments (not copied) */
	ctx->argc = src->argc;
	ctx->argv = src->argv;

	/* Copy program name if present */
	if (src->program_name != NULL) {
		ctx->program_name = strdup(src->program_name);
		if (ctx->program_name == NULL) {
			if (ctx->error_msg) {
				free(ctx->error_msg);
			}
			qd_stack_destroy(ctx->st);
			free(ctx);
			return NULL;
		}
	} else {
		ctx->program_name = NULL;
	}

	/* Copy call stack */
	ctx->call_stack_depth = src->call_stack_depth;
	for (size_t i = 0; i < src->call_stack_depth; i++) {
		ctx->call_stack[i] = src->call_stack[i];
		ctx->call_stack_files[i] = src->call_stack_files[i];
		ctx->call_stack_lines[i] = src->call_stack_lines[i];
	}

	return ctx;
}

// Call stack management for debugging/error reporting
void qd_push_call(qd_context* ctx, const char* func_name, const char* file, size_t line) {
	if (ctx->call_stack_depth < QD_MAX_CALL_STACK_DEPTH) {
		ctx->call_stack[ctx->call_stack_depth] = func_name;
		ctx->call_stack_files[ctx->call_stack_depth] = file;
		ctx->call_stack_lines[ctx->call_stack_depth] = line;
		ctx->call_stack_depth++;
	}
}

void qd_pop_call(qd_context* ctx) {
	if (ctx->call_stack_depth > 0) {
		ctx->call_stack_depth--;
	}
}

void qd_print_stack_trace(qd_context* ctx) {
	// Check NO_COLOR environment variable
	const bool use_color = getenv("NO_COLOR") == NULL;
	const char* color_start = use_color ? "\x1b[1;31m" : "";
	const char* color_end = use_color ? "\x1b[0m" : "";

	if (ctx->call_stack_depth == 0) {
		fprintf(stderr, "\n%sStack trace:%s (empty)\n", color_start, color_end);
		return;
	}
	fprintf(stderr, "\n%sStack trace:%s\n", color_start, color_end);
	for (size_t i = ctx->call_stack_depth; i > 0; i--) {
		size_t idx = i - 1;
		size_t frame_num = ctx->call_stack_depth - i;
		const char* func_name = ctx->call_stack[idx];
		const char* file = ctx->call_stack_files[idx];
		size_t line = ctx->call_stack_lines[idx];
		if (file && file[0] != '\0') {
			fprintf(stderr, "\t%zu: %s(%s:%zu)\n", frame_num, func_name, file, line);
		} else {
			fprintf(stderr, "\t%zu: %s\n", frame_num, func_name);
		}
	}
}

void qd_set_error_context(qd_context* ctx, const char* context) {
	if (ctx->error_context) {
		free(ctx->error_context);
	}
	ctx->error_context = context ? strdup(context) : NULL;
}

void qd_clear_error_context(qd_context* ctx) {
	if (ctx->error_context) {
		free(ctx->error_context);
		ctx->error_context = NULL;
	}
}

void qd_print_error_msg(qd_context* ctx, const char* func_name) {
	// Check NO_COLOR environment variable
	const bool use_color = getenv("NO_COLOR") == NULL;
	const char* color_red = use_color ? "\x1b[1;31m" : "";
	const char* color_dim = use_color ? "\x1b[2m" : "";
	const char* color_reset = use_color ? "\x1b[0m" : "";

	fprintf(stderr, "%sFatal error:%s ", color_red, color_reset);

	// Build error chain from call stack (bottom to top, i.e., main -> ... -> current)
	if (ctx->call_stack_depth > 0) {
		for (size_t i = 0; i < ctx->call_stack_depth; i++) {
			const char* name = ctx->call_stack[i];
			// Extract just the function name (after :: if present)
			const char* short_name = strrchr(name, ':');
			if (short_name && short_name > name && *(short_name - 1) == ':') {
				short_name++; // Skip past ::
			} else {
				short_name = name;
			}
			fprintf(stderr, "%s%s%s -> ", color_dim, short_name, color_reset);
		}
	}

	// Print the failing function name
	fprintf(stderr, "%s", func_name);

	// Print user-defined context if available, then the error message
	if (ctx->error_context && ctx->error_context[0] != '\0') {
		if (ctx->error_msg && ctx->error_msg[0] != '\0') {
			fprintf(stderr, " failed: %s: %s\n", ctx->error_context, ctx->error_msg);
		} else {
			fprintf(stderr, " failed: %s\n", ctx->error_context);
		}
		// Clear context after use
		free(ctx->error_context);
		ctx->error_context = NULL;
	} else if (ctx->error_msg && ctx->error_msg[0] != '\0') {
		fprintf(stderr, " failed: %s\n", ctx->error_msg);
	} else {
		fprintf(stderr, " failed\n");
	}
}

void qd_debug_print_stack(qd_context* ctx) {
	if (!ctx || !ctx->st) {
		fprintf(stderr, "Error: Invalid context");
		return;
	}

	size_t stack_size = qd_stack_size(ctx->st);

	// Check NO_COLOR environment variable
	const bool use_color = getenv("NO_COLOR") == NULL;
	const char* color_blue = use_color ? "\x1b[1;34m" : "";
	const char* color_green = use_color ? "\x1b[0;32m" : "";
	const char* color_yellow = use_color ? "\x1b[0;33m" : "";
	const char* color_cyan = use_color ? "\x1b[0;36m" : "";
	const char* color_end = use_color ? "\x1b[0m" : "";

	fprintf(stderr, "\n%s=== Data Stack ===%s\n", color_blue, color_end);
	fprintf(stderr, "Size: %zu element%s\n", stack_size, stack_size == 1 ? "" : "s");

	if (stack_size == 0) {
		fprintf(stderr, "(empty)");
		return;
	}

	fprintf(stderr, "\nTop of stack (most recent):");
	fprintf(stderr, "  %sIdx%s  %sType%s     %sValue%s\n", color_blue, color_end, color_green, color_end, color_yellow, color_end);
	fprintf(stderr, "  ----------------------------------------");

	// Print from top (most recent) to bottom
	for (size_t i = stack_size; i > 0; i--) {
		qd_stack_element_t elem;
		qd_stack_error err = qd_stack_element(ctx->st, i - 1, &elem);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "  [%zu]: <error reading element>\n", i - 1);
			continue;
		}

		fprintf(stderr, "  %s[%2zu]%s ", color_blue, i - 1, color_end);

		switch (elem.type) {
		case QD_STACK_TYPE_INT:
			fprintf(stderr, "%sint    %s %s%ld%s\n", color_green, color_end, color_yellow, elem.value.i, color_end);
			break;
		case QD_STACK_TYPE_FLOAT:
			fprintf(stderr, "%sfloat  %s %s%g%s\n", color_green, color_end, color_yellow, elem.value.f, color_end);
			break;
		case QD_STACK_TYPE_STR:
			if (elem.value.s) {
				// Truncate long strings
				if (strlen(qd_string_data(elem.value.s)) > 40) {
					fprintf(stderr, "%sstring %s %s\"%.37s...\"%s\n", color_green, color_end, color_cyan, qd_string_data(elem.value.s), color_end);
				} else {
					fprintf(stderr, "%sstring %s %s\"%s\"%s\n", color_green, color_end, color_cyan, qd_string_data(elem.value.s), color_end);
				}
			} else {
				fprintf(stderr, "%sstring %s %s<null>%s\n", color_green, color_end, color_cyan, color_end);
			}
			break;
		case QD_STACK_TYPE_PTR:
			fprintf(stderr, "%sptr    %s %s%p%s\n", color_green, color_end, color_yellow, elem.value.p, color_end);
			break;
		default:
			fprintf(stderr, "<unknown type %d>\n", elem.type);
			break;
		}
	}
	fprintf(stderr, "Bottom of stack (oldest)");
	fprintf(stderr, "");
}

// =============================================================================
// Exported helper functions for split runtime files (runtime_stack.c, etc.)
// =============================================================================

void qdrt_dump_stack(qd_context* ctx) {
	dump_stack(ctx);
}

bool qdrt_validate_binary_numeric_op(qd_context* ctx, const char* op_name) {
	return validate_binary_numeric_op(ctx, op_name);
}

qd_exec_result qdrt_pop_two_values(qd_context* ctx, qd_stack_element_t* a, qd_stack_element_t* b) {
	return pop_two_values(ctx, a, b);
}

qd_stack_error qdrt_push_element(qd_stack* stack, const qd_stack_element_t* elem) {
	return push_element(stack, elem);
}

// =============================================================================
// Public helper functions
// =============================================================================

void qd_dump_stack(qd_context* ctx) {
	dump_stack(ctx);
}

void qd_set_error_msg(qd_context* ctx, const char* msg) {
	if (ctx->error_msg) {
		free(ctx->error_msg);
	}
	ctx->error_msg = strdup(msg);
	// If strdup fails, error_msg will be NULL, which is acceptable
	// (error_code still indicates the error condition)
}

// =============================================================================
// Version Information
// =============================================================================

const char* qd_version(void) {
#ifdef QD_VERSION
	return QD_VERSION;
#else
	return "unknown";
#endif
}

int qd_version_api(void) {
#ifdef QD_VERSION_API
	return QD_VERSION_API;
#else
	return 0;
#endif
}

int qd_version_major(void) {
#ifdef QD_VERSION_MAJOR
	return QD_VERSION_MAJOR;
#else
	return 0;
#endif
}

int qd_version_minor(void) {
#ifdef QD_VERSION_MINOR
	return QD_VERSION_MINOR;
#else
	return 0;
#endif
}

int qd_version_patch(void) {
#ifdef QD_VERSION_PATCH
	return QD_VERSION_PATCH;
#else
	return 0;
#endif
}

qd_exec_result qd_rt_version(qd_context* ctx) {
	return qd_push_s(ctx, qd_version());
}

qd_exec_result qd_rt_version_api(qd_context* ctx) {
	return qd_push_i(ctx, qd_version_api());
}

// User-facing functions (usr_ prefix for import mechanism)
qd_exec_result usr_rt_version(qd_context* ctx) {
	return qd_push_s(ctx, qd_version());
}

qd_exec_result usr_rt_version_api(qd_context* ctx) {
	return qd_push_i(ctx, qd_version_api());
}

