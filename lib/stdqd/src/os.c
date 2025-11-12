#include <stdqd/os.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
#include <stdio.h>
#include <stdlib.h>

qd_exec_result qd_stdqd_exit(qd_context* ctx) {
	// Check stack has at least 1 element
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::exit: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the exit code
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::exit: Failed to pop exit code\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Verify it's an integer
	if (elem.type != QD_STACK_TYPE_INT) {
		fprintf(stderr, "Fatal error in os::exit: Expected integer exit code, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Exit the program with the given code
	exit((int)elem.value.i);

	// This line is never reached, but needed for compiler
	return (qd_exec_result){0};
}

qd_exec_result qd_stdqd_system(qd_context* ctx) {
	// Check stack has at least 1 element
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::system: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the command string
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::system: Failed to pop command string\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	// Verify it's a string
	if (elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::system: Expected string command, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Execute the command
	int exit_code = system(elem.value.s);

	// Free the string
	free(elem.value.s);

	// Push the exit code back onto the stack
	err = qd_stack_push_int(ctx->st, (int64_t)exit_code);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::system: Failed to push exit code\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
}

qd_exec_result qd_stdqd_getenv(qd_context* ctx) {
	size_t stack_size = qd_stack_size(ctx->st);
	if (stack_size < 1) {
		fprintf(stderr, "Fatal error in os::getenv: Stack underflow (required 1 element, have %zu)\n", stack_size);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Pop the variable name
	qd_stack_element_t elem;
	qd_stack_error err = qd_stack_pop(ctx->st, &elem);
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::getenv: Failed to pop variable name\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	if (elem.type != QD_STACK_TYPE_STR) {
		fprintf(stderr, "Fatal error in os::getenv: Expected string command, got type %d\n", elem.type);
		qd_print_stack_trace(ctx);
		abort();
	}

	// Get the environment variable
	const char* value = getenv(elem.value.s);
	err = qd_stack_push_str(ctx->st, value ? value : "");
	if (err != QD_STACK_OK) {
		fprintf(stderr, "Fatal error in os::getenv: Failed to push environment variable value\n");
		qd_print_stack_trace(ctx);
		abort();
	}

	return (qd_exec_result){0};
}
