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
    char name[256];
    int rc = qd_pop_s(ctx, name, sizeof(name));
    if (rc != QD_OK) {
        fprintf(stderr, "greet::hello: expected string argument\n");
        return 1;
    }

    // Print the greeting
    printf("Hello, %s!\n", name);

    // Return success (0 = no error)
    return 0;
}
