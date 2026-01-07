# FFI example

Demonstrates how to call C code from Quadrate using the Foreign Function Interface (FFI).

## Files

- `greet.c` - C library with a `hello` function that prints a greeting
- `main.qd` - Quadrate program that imports and calls the C function

## How FFI works

1. Write a C function with signature `qd_exec_result <function>(qd_context* ctx)`
2. Use `qd_stack_pop()` to get arguments, `qd_stack_push_*()` to return values
3. Compile to a static library (`.a` file)
4. Import in Quadrate with `import "libname.a" as "module" { ... }`

## Build

```bash
cd examples/ffi

# Compile the C file to an object file
cc -c greet.c -o greet.o -I../../dist/include

# Create a static library
ar rcs libgreet.a greet.o

# Compile and run the Quadrate program
# The library must be in QUADRATE_LIBDIR
QUADRATE_LIBDIR=.:../../dist/lib QUADRATE_ROOT=../../dist/share/quadrate \
    ../../build/debug/cmd/quadc/quadc main.qd -r
```

## Expected output

```
Hello, World!
Hello, Quadrate!
```

## Function naming

The C function name must match the function name in the import declaration:

```qd
import "libgreet.a" as "greet" {
    pub fn hello(name:str -- )  // C function must be named "hello"
}
```

The compiler generates a wrapper `usr_greet_hello` that calls your `hello` function.

## Stack operations

FFI functions manipulate the Quadrate stack directly:

```c
#include <qdrt/context.h>
#include <qdrt/exec_result.h>
#include <qdrt/stack.h>

qd_exec_result hello(qd_context* ctx) {
    // Pop a value from the stack
    qd_stack_element_t elem;
    qd_stack_pop(ctx->st, &elem);

    // Check the type and use the value
    if (elem.type == QD_STACK_TYPE_STR) {
        const char* str = qd_string_data(elem.value.s);
        printf("%s\n", str);
        qd_string_release(elem.value.s);  // Always release strings!
    }

    // Push a return value (if any)
    qd_stack_push_int(ctx->st, 42);

    return (qd_exec_result){0};  // 0 = success
}
```

## Available types

| Quadrate | C Field | Type Constant |
|----------|---------|---------------|
| `i64` | `elem.value.i` (`int64_t`) | `QD_STACK_TYPE_INT` |
| `f64` | `elem.value.f` (`double`) | `QD_STACK_TYPE_FLOAT` |
| `str` | `elem.value.s` (`qd_string*`) | `QD_STACK_TYPE_STR` |
| `ptr` | `elem.value.p` (`void*`) | `QD_STACK_TYPE_PTR` |

## Required headers

```c
#include <qdrt/context.h>      // qd_context
#include <qdrt/exec_result.h>  // qd_exec_result
#include <qdrt/stack.h>        // qd_stack_*, QD_STACK_TYPE_*
```
