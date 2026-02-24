// greet.c - Simple FFI example for Quadrate
//
// This file demonstrates how to write a C function that can be called
// from Quadrate code. The function pops a string from the stack and
// prints a greeting.

#include <quadrate/rt/ffi.h>
#include <stdio.h>

// FFI functions must follow this signature:
//   int <function>(qd_context* ctx)
//
// The function name matches what you declare in the Quadrate import block.
// The compiler generates a wrapper usr_<module>_<function> that calls this.

int hello(qd_context* ctx) {
    // Pop the string argument from the stack
    qd_stack_element_t elem;
    qd_stack_error err = qd_stack_pop(ctx->st, &elem);
    if (err != QD_STACK_OK) {
        fprintf(stderr, "greet::hello: stack underflow\n");
        return 1;
    }

    // Verify it's a string
    if (elem.type != QD_STACK_TYPE_STR) {
        fprintf(stderr, "greet::hello: expected string, got type %d\n", elem.type);
        return 1;
    }

    // Print the greeting
    printf("Hello, %s!\n", qd_string_data(elem.value.s));

    // Release the string (important for memory management)
    qd_string_release(elem.value.s);

    // Return success (0 = no error)
    return 0;
}
