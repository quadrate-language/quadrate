#include <qdbits/bits.h>
#include <qdrt/stack.h>
#include <qdrt/runtime.h>
#include <stdio.h>
#include <stdlib.h>

// and - bitwise/logical AND: ( a b -- a&b )
int usr_bits_and(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in bits::and: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in bits::and: Failed to access stack elements\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_a.type != QD_STACK_TYPE_INT || check_b.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in bits::and: Type error (expected int for bitwise operation)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	int64_t result = a.value.i & b.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// or - bitwise/logical OR: ( a b -- a|b )
int usr_bits_or(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in bits::or: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in bits::or: Failed to access stack elements\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_a.type != QD_STACK_TYPE_INT || check_b.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in bits::or: Type error (expected int for bitwise operation)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	int64_t result = a.value.i | b.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// xor - bitwise XOR: ( a b -- a^b )
int usr_bits_xor(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in bits::xor: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_b, check_a;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_b);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_a);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in bits::xor: Failed to access stack elements\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_a.type != QD_STACK_TYPE_INT || check_b.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in bits::xor: Type error (expected int for bitwise operation)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t b, a;
	qd_stack_error err = qd_stack_pop(ctx->st, &b);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	err = qd_stack_pop(ctx->st, &a);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	int64_t result = a.value.i ^ b.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// not - bitwise/logical NOT: ( a -- ~a )
int usr_bits_not(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in bits::not: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_val;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_val);
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in bits::not: Failed to access stack element\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_val.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in bits::not: Type error (expected int for bitwise operation)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t val;
	qd_stack_error err = qd_stack_pop(ctx->st, &val);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	int64_t result = ~val.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// lshift - logical shift left: ( x n -- x<<n )
int usr_bits_lshift(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in bits::lshift: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_n, check_x;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_n);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_x);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in bits::lshift: Failed to access stack elements\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_x.type != QD_STACK_TYPE_INT || check_n.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in bits::lshift: Type error (expected int for bitwise operation)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t n, x;
	qd_stack_error err = qd_stack_pop(ctx->st, &n);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	err = qd_stack_pop(ctx->st, &x);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	// Check for negative or excessive shift counts
	if (n.value.i < 0 || n.value.i >= 64) {
		fprintf(stderr, "Fatal error in bits::lshift: Shift count out of range (must be 0-63, got %ld)\n", n.value.i);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	int64_t result = x.value.i << n.value.i;

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}

// rshift - logical shift right: ( x n -- x>>n )
int usr_bits_rshift(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 2) {
		fprintf(stderr, "Fatal error in bits::rshift: Stack underflow (required 2 elements, have %zu)\n", stack_size);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t check_n, check_x;
	qd_stack_error check_err = qd_stack_element(ctx->st, stack_size - 1, &check_n);
	if (check_err == QD_STACK_OK) {
		check_err = qd_stack_element(ctx->st, stack_size - 2, &check_x);
	}
	if (check_err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in bits::rshift: Failed to access stack elements\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	if (check_x.type != QD_STACK_TYPE_INT || check_n.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in bits::rshift: Type error (expected int for bitwise operation)\n");
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	qd_stack_element_t n, x;
	qd_stack_error err = qd_stack_pop(ctx->st, &n);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}
	err = qd_stack_pop(ctx->st, &x);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	// Check for negative or excessive shift counts
	if (n.value.i < 0 || n.value.i >= 64) {
		fprintf(stderr, "Fatal error in bits::rshift: Shift count out of range (must be 0-63, got %ld)\n", n.value.i);
		qd_dump_stack(ctx);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Logical shift right (unsigned)
	int64_t result = (int64_t)((uint64_t)x.value.i >> n.value.i);

	err = qd_stack_push_int(ctx->st, result);
	if (err != QD_STACK_OK) {
		return (int){-2};
	}

	return (int){0};
}
