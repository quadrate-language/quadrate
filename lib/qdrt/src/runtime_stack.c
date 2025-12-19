// Stack manipulation operations for Quadrate runtime
// Split from runtime.c for maintainability

#define _POSIX_C_SOURCE 200809L

#include <qdrt/runtime.h>
#include <qdrt/qd_string.h>
#include <stdio.h>
#include <stdlib.h>
#include "runtime_internal.h"

qd_exec_result qd_dup(qd_context* ctx) {
	// Duplicate the top element of the stack
	qd_stack* st = ctx->st;

	// Fast path: direct struct access
	if (st->size >= 1 && st->size < st->capacity) {
		qd_stack_element_t* top = &st->data[st->size - 1];

		if (top->type == QD_STACK_TYPE_INT) {
			// Fast integer dup - most common case
			QD_STACK_PUSH_INT_FAST(st, top->value.i);
			return (qd_exec_result){0};
		}
		if (top->type == QD_STACK_TYPE_FLOAT) {
			// Fast float dup
			QD_STACK_PUSH_FLOAT_FAST(st, top->value.f);
			return (qd_exec_result){0};
		}
		if (top->type == QD_STACK_TYPE_STR) {
			// String dup - retain and copy
			qd_string_retain(top->value.s);
			st->data[st->size].value.s = top->value.s;
			st->data[st->size].type = QD_STACK_TYPE_STR;
			st->data[st->size].is_error_tainted = top->is_error_tainted;
			st->size++;
			return (qd_exec_result){0};
		}
	}

	// Slow path with full error checking
	size_t stack_size = st->size;
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in dup: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t top;
	qd_stack_error err = qd_stack_peek(st, &top);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dup: Failed to peek stack\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push a copy of the top element (strings are retained, not copied)
	err = qdrt_push_element(st, &top);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_dupd(qd_context* ctx) {
	// Duplicate the second element of the stack: ( a b -- a a b )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in dupd: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop top two elements
	qd_stack_element_t a, b;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);  // b is top
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dupd: Failed to pop top element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	err = qd_stack_pop(ctx->st, &a);  // a is second
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dupd: Failed to pop second element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push back: a, a, b (strings are retained, not copied)
	// Push first a
	err = qdrt_push_element(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dupd: Failed to push first copy of second element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		abort();
	}

	// Push second a (duplicate)
	err = qdrt_push_element(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dupd: Failed to push second copy of second element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		abort();
	}

	// Release our reference to 'a' (push_element retained it twice)
	qdrt_release_if_string(&a);

	// Push b (top element)
	err = qdrt_push_element(ctx->st, &b);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dupd: Failed to push top element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		qdrt_release_if_string(&b);
		abort();
	}

	// Release our reference to 'b' (push_element retained it)
	qdrt_release_if_string(&b);

	return (qd_exec_result){0};
}

qd_exec_result qd_dup2(qd_context* ctx) {
	// Duplicate the top two elements of the stack: ( a b -- a b a b )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in dup2: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the second element (index stack_size - 2)
	qd_stack_element_t second;
	qd_stack_error err = qd_stack_element(ctx->st, stack_size - 2, &second);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dup2: Failed to access second element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the top element (index stack_size - 1)
	qd_stack_element_t top;
	err = qd_stack_element(ctx->st, stack_size - 1, &top);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in dup2: Failed to access top element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push a copy of the second element (strings are retained, not copied)
	err = qdrt_push_element(ctx->st, &second);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push a copy of the top element
	err = qdrt_push_element(ctx->st, &top);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_swap(qd_context* ctx) {
	// Swap the top two elements of the stack
	qd_stack* st = ctx->st;

	// Fast path: direct in-place swap for numeric types
	if (st->size >= 2) {
		qd_stack_element_t* top = &st->data[st->size - 1];
		qd_stack_element_t* second = &st->data[st->size - 2];

		// For numeric types, just swap in place - no refcount concerns
		if (top->type != QD_STACK_TYPE_STR && second->type != QD_STACK_TYPE_STR) {
			qd_stack_element_t tmp = *top;
			*top = *second;
			*second = tmp;
			return (qd_exec_result){0};
		}
	}

	// Slow path with full error checking (handles strings)
	size_t stack_size = st->size;
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in swap: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop top two elements
	qd_stack_element_t a, b;
	qd_stack_error err = qd_stack_pop(st, &b);  // b is top
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(st, &a);  // a is second
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push them back in swapped order (b first, then a, strings are retained not copied)
	err = qdrt_push_element(st, &b);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		return (qd_exec_result){-2};
	}

	err = qdrt_push_element(st, &a);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		return (qd_exec_result){-2};
	}

	// Release our original references (push_element retained them)
	qdrt_release_if_string(&a);
	qdrt_release_if_string(&b);

	return (qd_exec_result){0};
}

qd_exec_result qd_swapd(qd_context* ctx) {
	// Swap second and third elements: ( a b c -- b a c )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in swapd: Stack underflow (required 3 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop top three elements
	qd_stack_element_t a, b, c;
	qd_stack_error err = qd_stack_pop(ctx->st, &c);  // c is top
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in swapd: Failed to pop top element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	err = qd_stack_pop(ctx->st, &b);  // b is second
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in swapd: Failed to pop second element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	err = qd_stack_pop(ctx->st, &a);  // a is third
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in swapd: Failed to pop third element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push back: b, a, c (swapped second and third, strings are retained not copied)
	err = qdrt_push_element(ctx->st, &b);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in swapd: Failed to push b\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		qdrt_release_if_string(&c);
		abort();
	}

	err = qdrt_push_element(ctx->st, &a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in swapd: Failed to push a\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		qdrt_release_if_string(&c);
		abort();
	}

	err = qdrt_push_element(ctx->st, &c);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in swapd: Failed to push c\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		qdrt_release_if_string(&c);
		abort();
	}

	// Release our original references (push_element retained them)
	qdrt_release_if_string(&a);
	qdrt_release_if_string(&b);
	qdrt_release_if_string(&c);

	return (qd_exec_result){0};
}

qd_exec_result qd_swap2(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 4) {
		fprintf(stderr, "Fatal error in swap2: Stack underflow (required 4 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop d, c, b, a
	qd_stack_element_t d, c, b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &d);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &c);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&d);
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&d);
		qdrt_release_if_string(&c);
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&d);
		qdrt_release_if_string(&c);
		qdrt_release_if_string(&b);
		return (qd_exec_result){-2};
	}

	// Push in order: c, d, a, b (strings are retained, not copied)
	err = qdrt_push_element(ctx->st, &c);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		qdrt_release_if_string(&c);
		qdrt_release_if_string(&d);
		return (qd_exec_result){-2};
	}

	err = qdrt_push_element(ctx->st, &d);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		qdrt_release_if_string(&c);
		qdrt_release_if_string(&d);
		return (qd_exec_result){-2};
	}

	err = qdrt_push_element(ctx->st, &a);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		qdrt_release_if_string(&c);
		qdrt_release_if_string(&d);
		return (qd_exec_result){-2};
	}

	err = qdrt_push_element(ctx->st, &b);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		qdrt_release_if_string(&c);
		qdrt_release_if_string(&d);
		return (qd_exec_result){-2};
	}

	// Release our original references (push_element retained them)
	qdrt_release_if_string(&a);
	qdrt_release_if_string(&b);
	qdrt_release_if_string(&c);
	qdrt_release_if_string(&d);

	return (qd_exec_result){0};
}

qd_exec_result qd_over(qd_context* ctx) {
	// Copy the second element to the top: ( a b -- a b a )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in over: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the second element (without popping)
	qd_stack_element_t second;
	qd_stack_error err = qd_stack_element(ctx->st, stack_size - 2, &second);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in over: Failed to access second element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push a copy of the second element to the top (strings are retained, not copied)
	err = qdrt_push_element(ctx->st, &second);

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_overd(qd_context* ctx) {
	// Copy third element between second and top: ( a b c -- a b a c )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in overd: Stack underflow (required 3 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the top element
	qd_stack_element_t top;
	qd_stack_error err = qd_stack_pop(ctx->st, &top);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in overd: Failed to pop top element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the third element (now at index stack_size - 3, but stack is smaller by 1)
	qd_stack_element_t third;
	err = qd_stack_element(ctx->st, stack_size - 3, &third);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in overd: Failed to access third element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push a copy of the third element (strings are retained, not copied)
	err = qdrt_push_element(ctx->st, &third);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in overd: Failed to push copy of third element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push the top element back (strings are retained, not copied)
	err = qdrt_push_element(ctx->st, &top);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in overd: Failed to push top element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		qdrt_release_if_string(&top);
		abort();
	}
	// Release our original reference (push_element retained it)
	qdrt_release_if_string(&top);

	return (qd_exec_result){0};
}

qd_exec_result qd_over2(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 4) {
		fprintf(stderr, "Fatal error in over2: Stack underflow (required 4 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the second pair (indices stack_size-4 and stack_size-3)
	qd_stack_element_t elem_a, elem_b;
	qd_stack_error err = qd_stack_element(ctx->st, stack_size - 4, &elem_a);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in over2: Failed to access element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	err = qd_stack_element(ctx->st, stack_size - 3, &elem_b);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in over2: Failed to access element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push copies of elem_a and elem_b
	err = qdrt_push_element(ctx->st, &elem_a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	err = qdrt_push_element(ctx->st, &elem_b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_nip(qd_context* ctx) {
	// Remove the second element: ( a b -- b )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in nip: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the top element
	qd_stack_element_t top;
	qd_stack_error err = qd_stack_pop(ctx->st, &top);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Pop the second element (which we want to discard)
	qd_stack_element_t second;
	err = qd_stack_pop(ctx->st, &second);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Release string reference if necessary
	if (second.type == QD_STACK_TYPE_STR) {
		qd_string_release(second.value.s);
	}

	// Push the top element back
	err = qdrt_push_element(ctx->st, &top);

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Release our reference to top (push_element retained it)
	if (top.type == QD_STACK_TYPE_STR) {
		qd_string_release(top.value.s);
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_nipd(qd_context* ctx) {
	// Remove second element: ( a b c -- a c )
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in nipd: Stack underflow (required 3 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop top two elements
	qd_stack_element_t b, c;
	qd_stack_error err = qd_stack_pop(ctx->st, &c);  // c is top
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in nipd: Failed to pop top element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}
	err = qd_stack_pop(ctx->st, &b);  // b is second (to be removed)
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in nipd: Failed to pop second element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Release b's string reference if it's a string
	if (b.type == QD_STACK_TYPE_STR) {
		qd_string_release(b.value.s);
	}

	// Push c back
	err = qdrt_push_element(ctx->st, &c);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in nipd: Failed to push top element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_drop(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in drop: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Release string reference if needed
	if (val.type == QD_STACK_TYPE_STR) {
		qd_string_release(val.value.s);
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_drop2(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in drop2: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t val;
	// Drop first element
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	if (val.type == QD_STACK_TYPE_STR) {
		qd_string_release(val.value.s);
	}

	// Drop second element
	err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	if (val.type == QD_STACK_TYPE_STR) {
		qd_string_release(val.value.s);
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_rot(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 3) {
		fprintf(stderr, "Fatal error in rot: Stack underflow (required 3 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop c, b, a
	qd_stack_element_t c, b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &c);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push in order: b, c, a
	// Push b
	err = qdrt_push_element(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push c
	err = qdrt_push_element(ctx->st, &c);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push a
	err = qdrt_push_element(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Release our original references (push_element retained them)
	if (a.type == QD_STACK_TYPE_STR) {
		qd_string_release(a.value.s);
	}
	if (b.type == QD_STACK_TYPE_STR) {
		qd_string_release(b.value.s);
	}
	if (c.type == QD_STACK_TYPE_STR) {
		qd_string_release(c.value.s);
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_tuck(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in tuck: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop b and a
	qd_stack_element_t b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	// Push in order: b, a, b
	// Push b (first copy)
	err = qdrt_push_element(ctx->st, &b);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&b);
		qdrt_release_if_string(&a);
		return (qd_exec_result){-2};
	}

	// Push a (strings are retained, not copied)
	err = qdrt_push_element(ctx->st, &a);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		return (qd_exec_result){-2};
	}

	// Push b (second copy, strings are retained)
	err = qdrt_push_element(ctx->st, &b);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&a);
		qdrt_release_if_string(&b);
		return (qd_exec_result){-2};
	}

	// Release our original references (push_element retained them twice for b)
	qdrt_release_if_string(&a);
	qdrt_release_if_string(&b);

	// Dummy switch to maintain structure (will be removed)
	switch (b.type) {
		case QD_STACK_TYPE_INT:
		case QD_STACK_TYPE_FLOAT:
		case QD_STACK_TYPE_STR:
		case QD_STACK_TYPE_PTR:
			break;
		default:
			qdrt_release_if_string(&b);
			return (qd_exec_result){-3};
	}
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_pick(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in pick: Stack underflow (need at least the index)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the index
	qd_stack_element_t idx_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &idx_elem);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	if (idx_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in pick: Index must be an integer\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t n = idx_elem.value.i;
	if (n < 0) {
		fprintf(stderr, "Fatal error in pick: Index must be non-negative (got %ld)\n", n);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	stack_size = qd_stack_size(ctx->st);  // Update after popping index
	if ((size_t)n >= stack_size) {
		fprintf(stderr, "Fatal error in pick: Index %ld out of range (stack has %zu elements)\n", n, stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the nth element from the top (0 = top)
	qd_stack_element_t elem;
	err = qd_stack_element(ctx->st, stack_size - 1 - (size_t)n, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in pick: Failed to access element\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Push a copy of that element
	err = qdrt_push_element(ctx->st, &elem);

	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_roll(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in roll: Stack underflow (need at least the count)\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the count
	qd_stack_element_t count_elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &count_elem);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	if (count_elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in roll: Count must be an integer\n");
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t n = count_elem.value.i;
	if (n < 0) {
		fprintf(stderr, "Fatal error in roll: Count must be non-negative (got %ld)\n", n);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (n == 0) {
		return (qd_exec_result){0};  // Nothing to do
	}

	stack_size = qd_stack_size(ctx->st);  // Update after popping count
	if ((size_t)n > stack_size) {
		fprintf(stderr, "Fatal error in roll: Count %ld exceeds stack size %zu\n", n, stack_size);
		qdrt_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Allocate temporary storage for n elements
	qd_stack_element_t* temp = malloc(sizeof(qd_stack_element_t) * (size_t)n);
	if (!temp) {
		fprintf(stderr, "Fatal error in roll: Memory allocation failed\n");
		abort();
	}

	// Pop n elements
	for (int64_t i = 0; i < n; i++) {
		err = qd_stack_pop(ctx->st, &temp[n - 1 - i]);
		if (err != QD_STACK_OK) {
			free(temp);
			return (qd_exec_result){-2};
		}
	}

	// Push back in rotated order: the bottom element (temp[0]) goes to top
	// Order after: temp[1], temp[2], ..., temp[n-1], temp[0]
	// (strings are retained, not copied)
	for (int64_t i = 1; i < n; i++) {
		err = qdrt_push_element(ctx->st, &temp[i]);
		if (err != QD_STACK_OK) {
			// Release all remaining elements
			for (int64_t j = i; j < n; j++) {
				qdrt_release_if_string(&temp[j]);
			}
			qdrt_release_if_string(&temp[0]);
			free(temp);
			return (qd_exec_result){-2};
		}
		// Release our reference (push_element retained it)
		qdrt_release_if_string(&temp[i]);
	}

	// Push temp[0] last (it becomes the top)
	err = qdrt_push_element(ctx->st, &temp[0]);
	if (err != QD_STACK_OK) {
		qdrt_release_if_string(&temp[0]);
		free(temp);
		return (qd_exec_result){-2};
	}
	// Release our reference (push_element retained it)
	qdrt_release_if_string(&temp[0]);

	free(temp);

	return (qd_exec_result){0};
}

qd_exec_result qd_clear(qd_context* ctx) {
	// Pop all elements from the stack until empty
	qd_stack_element_t elem;
	while (!qd_stack_is_empty(ctx->st)) {
		qd_stack_error err = qd_stack_pop(ctx->st, &elem);
		if (err != QD_STACK_OK) {
			fprintf(stderr, "Fatal error in clear: Failed to pop element\n");
			qdrt_dump_stack(ctx);
			abort();
		}
		// Release string reference if it was a string element
		if (elem.type == QD_STACK_TYPE_STR) {
			qd_string_release(elem.value.s);
		}
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_depth(qd_context* ctx) {
	// Get current stack size and push it as an integer
	size_t stack_size = qd_stack_size(ctx->st);

	qd_stack_error err = qd_stack_push_int(ctx->st, (int64_t)stack_size);
	if (err != QD_STACK_OK) {
		return (qd_exec_result){-2};
	}

	return (qd_exec_result){0};
}

