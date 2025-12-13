# Foreign Function Interface (FFI)

Quadrate can call C functions directly through its Foreign Function Interface. This allows you to:

- Use existing C libraries
- Write performance-critical code in C
- Interface with system APIs
- Create custom native modules

## Basic Example

Let's create a simple C function that Quadrate can call.

### Step 1: Write the C Code

Create `greet.c`:

```c
#include <qdrt/context.h>
#include <qdrt/exec_result.h>
#include <qdrt/stack.h>
#include <stdio.h>

qd_exec_result hello(qd_context* ctx) {
    // Pop a string from the stack
    qd_stack_element_t elem;
    qd_stack_pop(ctx->st, &elem);

    // Print the greeting
    printf("Hello, %s!\n", qd_string_data(elem.value.s));

    // Release the string (required for memory management)
    qd_string_release(elem.value.s);

    return (qd_exec_result){0};  // 0 = success
}
```

### Step 2: Compile to a Static Library

```bash
cc -c greet.c -o greet.o -I/usr/include
ar rcs libgreet.a greet.o
```

### Step 3: Import in Quadrate

Create `main.qd`:

```qd
import "libgreet.a" as "greet" {
    pub fn hello(name:str -- )
}

fn main( -- ) {
    "World" greet::hello
}
```

### Step 4: Run

```bash
QUADRATE_LIBDIR=. quadc main.qd -r
```

Output:
```
Hello, World!
```

## Function Signature

All FFI functions must have this signature:

```c
qd_exec_result function_name(qd_context* ctx)
```

The function name in C must match the name declared in the Quadrate import block. The compiler generates a wrapper with the `usr_<module>_<function>` naming convention.

## Required Headers

```c
#include <qdrt/context.h>      // qd_context type
#include <qdrt/exec_result.h>  // qd_exec_result type
#include <qdrt/stack.h>        // Stack operations
```

## Stack Operations

### Popping Values

```c
qd_stack_element_t elem;
qd_stack_error err = qd_stack_pop(ctx->st, &elem);

if (err != QD_STACK_OK) {
    // Handle stack underflow
    return (qd_exec_result){1};
}

// Check the type
switch (elem.type) {
    case QD_STACK_TYPE_INT:
        int64_t i = elem.value.i;
        break;
    case QD_STACK_TYPE_FLOAT:
        double f = elem.value.f;
        break;
    case QD_STACK_TYPE_STR:
        const char* s = qd_string_data(elem.value.s);
        // Don't forget to release!
        qd_string_release(elem.value.s);
        break;
    case QD_STACK_TYPE_PTR:
        void* p = elem.value.p;
        break;
}
```

### Pushing Values

```c
// Push an integer
qd_stack_push_int(ctx->st, 42);

// Push a float
qd_stack_push_float(ctx->st, 3.14);

// Push a string (copies the string)
qd_stack_push_str(ctx->st, "hello");

// Push a pointer
qd_stack_push_ptr(ctx->st, some_pointer);
```

## Type Mapping

| Quadrate | C Type | Stack Field | Type Constant |
|----------|--------|-------------|---------------|
| `i64` | `int64_t` | `elem.value.i` | `QD_STACK_TYPE_INT` |
| `f64` | `double` | `elem.value.f` | `QD_STACK_TYPE_FLOAT` |
| `str` | `qd_string*` | `elem.value.s` | `QD_STACK_TYPE_STR` |
| `ptr` | `void*` | `elem.value.p` | `QD_STACK_TYPE_PTR` |

## Complete Example: Math Operations

Here's a more complete example with multiple functions and return values.

### math_ext.c

```c
#include <qdrt/context.h>
#include <qdrt/exec_result.h>
#include <qdrt/stack.h>
#include <math.h>

// Calculate hypotenuse: ( a:f64 b:f64 -- c:f64 )
qd_exec_result hypot(qd_context* ctx) {
    qd_stack_element_t b, a;
    qd_stack_pop(ctx->st, &b);  // Pop b (top)
    qd_stack_pop(ctx->st, &a);  // Pop a (below b)

    double result = sqrt(a.value.f * a.value.f + b.value.f * b.value.f);
    qd_stack_push_float(ctx->st, result);

    return (qd_exec_result){0};
}

// Factorial: ( n:i64 -- result:i64 )
qd_exec_result factorial(qd_context* ctx) {
    qd_stack_element_t elem;
    qd_stack_pop(ctx->st, &elem);

    int64_t n = elem.value.i;
    int64_t result = 1;
    for (int64_t i = 2; i <= n; i++) {
        result *= i;
    }

    qd_stack_push_int(ctx->st, result);
    return (qd_exec_result){0};
}
```

### main.qd

```qd
import "libmath_ext.a" as "mathx" {
    pub fn hypot(a:f64 b:f64 -- c:f64)
    pub fn factorial(n:i64 -- result:i64)
}

use fmt

fn main( -- ) {
    // Calculate hypotenuse of 3-4-5 triangle
    3.0 4.0 mathx::hypot "%f\n" fmt::printf  // 5.0

    // Calculate 10!
    10 mathx::factorial "%d\n" fmt::printf   // 3628800
}
```

## Error Handling

Return a non-zero error code to indicate failure:

```c
qd_exec_result my_function(qd_context* ctx) {
    qd_stack_element_t elem;
    qd_stack_error err = qd_stack_pop(ctx->st, &elem);

    if (err != QD_STACK_OK) {
        fprintf(stderr, "my_function: stack underflow\n");
        return (qd_exec_result){1};  // Error code 1
    }

    if (elem.type != QD_STACK_TYPE_INT) {
        fprintf(stderr, "my_function: expected integer\n");
        return (qd_exec_result){2};  // Error code 2
    }

    // ... do work ...

    return (qd_exec_result){0};  // Success
}
```

In Quadrate, use the `!` suffix for failable functions:

```qd
import "libmylib.a" as "mylib" {
    pub fn my_function(x:i64 -- result:i64)!
}

fn main( -- ) {
    42 mylib::my_function! -> result
}
```

## Memory Management

**Important:** Always release strings after use:

```c
qd_stack_element_t elem;
qd_stack_pop(ctx->st, &elem);

if (elem.type == QD_STACK_TYPE_STR) {
    const char* str = qd_string_data(elem.value.s);
    // ... use the string ...
    qd_string_release(elem.value.s);  // Required!
}
```

Failure to release strings will cause memory leaks.

## Build Integration

For larger projects, add FFI libraries to your build system:

### Makefile

```makefile
CFLAGS = -I$(QUADRATE_PREFIX)/include
LDFLAGS = -L$(QUADRATE_PREFIX)/lib

libmylib.a: mylib.o
	ar rcs $@ $^

mylib.o: mylib.c
	$(CC) $(CFLAGS) -c $< -o $@
```

### Meson

```meson
mylib = static_library('mylib', 'mylib.c',
    include_directories: include_directories('/usr/include'))
```

## Tips

1. **Check stack size** before popping to avoid crashes
2. **Validate types** - don't assume the stack contains what you expect
3. **Release strings** - memory leaks are easy to introduce
4. **Use descriptive error messages** - they help debugging
5. **Keep functions small** - easier to test and maintain

## See Also

- [Memory Management](memory.md) - Understanding Quadrate's memory model
- [Function Pointers](function-pointers.md) - Passing functions as values
- [examples/ffi](https://git.sr.ht/~klahr/quadrate/tree/master/item/examples/ffi) - Complete working example
