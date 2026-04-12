# Foreign function interface (FFI)

Quadrate can call C functions directly through its Foreign Function Interface. This allows you to:

- Use existing C libraries
- Write performance-critical code in C
- Interface with system APIs
- Create custom native modules

## Basic example

Let's create a simple C function that Quadrate can call.

### Step 1: write the C code

Create `greet.c`:

```c
#include <quadrate/rt/ffi.h>
#include <stdio.h>

int hello(qd_context* ctx) {
	// Pop the string argument from the stack
	char name[256];
	qd_pop_s(ctx, name, sizeof(name));

	// Print the greeting
	printf("Hello, %s!\n", name);

	return QD_OK;
}
```

### Step 2: compile to a static library

```bash
cc -c greet.c -o greet.o -I/usr/include
ar rcs libgreet.a greet.o
```

### Step 3: import in Quadrate

Create `main.qd`:

```qd
import "libgreet.a" as "greet" {
	pub fn hello(name:str -- )
}

fn main() {
	"Seb" greet::hello
}
```

### Step 4: run

```bash
quad run main.qd
```

Output:
```
Hello, Seb!
```

## Function signature

All FFI functions must have this signature:

```c
int function_name(qd_context* ctx)
```

The function name in C must match the name declared in the Quadrate import block.

## Required header

```c
#include <quadrate/rt/ffi.h>  // All FFI types and functions
```

## Stack operations

### Popping values

```c
// Pop a typed value — returns QD_OK on success, error code on failure
int64_t i;
qd_pop_i(ctx, &i);

double f;
qd_pop_f(ctx, &f);

char buf[256];
qd_pop_s(ctx, buf, sizeof(buf));  // copies string into buffer, releases it

void* p;
qd_pop_p(ctx, &p);
```

### Pushing values

```c
qd_push_i(ctx, 42);
qd_push_f(ctx, 3.14);
qd_push_s(ctx, "hello");
qd_push_p(ctx, some_pointer);
```

## Type mapping

| Quadrate | C Type | Pop | Push |
|----------|--------|-----|------|
| `i64` | `int64_t` | `qd_pop_i(ctx, &val)` | `qd_push_i(ctx, val)` |
| `f64` | `double` | `qd_pop_f(ctx, &val)` | `qd_push_f(ctx, val)` |
| `str` | `char*` | `qd_pop_s(ctx, buf, size)` | `qd_push_s(ctx, str)` |
| `ptr` | `void*` | `qd_pop_p(ctx, &val)` | `qd_push_p(ctx, ptr)` |

## Complete example: math operations

Here's a more complete example with multiple functions and return values.

### math_ext.c

```c
#include <quadrate/rt/ffi.h>
#include <math.h>

// Calculate hypotenuse: ( a:f64 b:f64 -- c:f64 )
int hypot(qd_context* ctx) {
	double b, a;
	qd_pop_f(ctx, &b);
	qd_pop_f(ctx, &a);
	return qd_push_f(ctx, sqrt(a * a + b * b));
}

// Factorial: ( n:i64 -- result:i64 )
int factorial(qd_context* ctx) {
	int64_t n;
	qd_pop_i(ctx, &n);

	int64_t result = 1;
	for (int64_t i = 2; i <= n; i++) {
		result *= i;
	}

	return qd_push_i(ctx, result);
}
```

### main.qd

```qd
import "libmath_ext.a" as "mathx" {
	pub fn hypot(a:f64 b:f64 -- c:f64)
	pub fn factorial(n:i64 -- result:i64)
}

use fmt

fn main() {
	// Calculate hypotenuse of 3-4-5 triangle
	3.0 4.0 mathx::hypot "%f\n" fmt::printf  // 5.0

	// Calculate 10!
	10 mathx::factorial "%d\n" fmt::printf   // 3628800
}
```

## Error handling

Return a non-zero error code to indicate failure. The `qd_pop_*` functions
return error codes for underflow and type mismatch:

```c
int my_function(qd_context* ctx) {
	int64_t val;
	int rc = qd_pop_i(ctx, &val);
	if (rc != QD_OK) {
		fprintf(stderr, "my_function: expected integer\n");
		return rc;
	}

	// ... do work ...

	return QD_OK;
}
```

In Quadrate, use the `!` suffix for failable functions. Define error code constants at module top-level alongside the import block:

```qd
pub const ErrOverflow = 2
pub const ErrTypeMismatch = 3

import "libmylib.a" as "mylib" {
	pub fn my_function(x:i64 -- result:i64)!
}

fn main() {
	42 mylib::my_function! -> result
}
```

The constants are accessible via `mylib::ErrOverflow` from any module that uses `mylib`.

## Memory management

The `qd_pop_s` function copies the string into your buffer and releases
the reference-counted string automatically. No manual cleanup needed:

```c
char buf[256];
qd_pop_s(ctx, buf, sizeof(buf));
// buf is a plain C string — use it freely, no release needed
```

## Build integration

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

1. **Check return codes** - `qd_pop_*` returns non-zero on underflow or type mismatch
2. **Use typed pop functions** - `qd_pop_i`, `qd_pop_f`, `qd_pop_s`, `qd_pop_p` handle type checking and string cleanup for you
3. **Use descriptive error messages** - they help debugging
4. **Keep functions small** - easier to test and maintain

## What's next?

Explore [Examples](../8-examples/file-processing.md) to see complete programs.
