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
cc -c greet.c -o greet.o -I"$QUADRATE_PREFIX/include"
ar rcs libgreet.a greet.o
```

`QUADRATE_PREFIX` is where Quadrate is installed, such as `/usr/local`, or `dist` in a source
checkout; the headers are in its `include` directory.

### Step 3: import in Quadrate

Create `main.qd`:

<!-- doccheck: skip links against an external .a built by the reader -->
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
| `ptr` | `void*` (raw memory) | `qd_pop_p(ctx, &val)` | `qd_push_p(ctx, ptr)` |

## Complete example: math operations

Here's a more complete example with multiple functions and return values.

### math_ext.c

```c
#include <quadrate/rt/ffi.h>
#include <math.h>

// Calculate hypotenuse: ( a:f64 b:f64 -- c:f64 )
// Not named hypot: that name is taken by the C library's math.h
int hypotenuse(qd_context* ctx) {
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

<!-- doccheck: skip links against an external .a built by the reader -->
```qd
import "libmath_ext.a" as "mathx" {
	pub fn hypotenuse(a:f64 b:f64 -- c:f64)
	pub fn factorial(n:i64 -- result:i64)
}

fn main() {
	// Calculate hypotenuse of 3-4-5 triangle
	3.0 4.0 mathx::hypotenuse print nl  // 5

	// Calculate 10!
	10 mathx::factorial print nl        // 3628800
}
```

## Error handling

A native that can fail is declared with `!` in the import block, and the caller handles it like
any other fallible call: `!` to abort, `?` to propagate, or `ok if`/`fail if` or `switch`.

The C side has a fixed contract. On success, push the outputs and then a status `1`; the
script sees the call succeed with `Ok` (0). On failure, return `qd_fail`, which records the
error code and message and pushes the status itself. The code must not be `0` or `1`.

```c
#include <quadrate/rt/ffi.h>
#include <math.h>

#define ERR_NEGATIVE 2

// int_sqrt( x:i64 -- root:i64 )!
int int_sqrt(qd_context* ctx) {
	int64_t x;
	qd_args(ctx, "mathx::int_sqrt", "i", &x);
	if (x < 0) {
		return qd_fail(ctx, ERR_NEGATIVE, "int_sqrt: %lld is negative", (long long)x);
	}
	qd_push_i(ctx, (int64_t)sqrt((double)x));  // the output
	qd_push_i(ctx, 1);                         // the status: success (the script sees Ok)
	return 0;
}
```

`qd_args` pops every argument at once and checks their types first; it is the safer choice
once a function takes more than one argument.

Define the error codes as constants at module top level, beside the import block:

<!-- doccheck: skip links against an external .a built by the reader -->
```qd
pub const ErrNegative = 2

import "libmath_ext.a" as "mathx" {
	pub fn int_sqrt(x:i64 -- root:i64)!
}

fn main() {
	16 mathx::int_sqrt! print nl  // 4

	-4 mathx::int_sqrt ok if {
		print nl
	} else {
		drop                       // the placeholder output
		err -> code -> msg
		msg print nl               // int_sqrt: -4 is negative
	}
}
```

The constants are accessible via `mathx::ErrNegative` from any module that uses `mathx`.

## Pointers, structs and function values

A `ptr` is raw memory: something from `malloc`, a buffer, a C handle. `qd_push_p` and
`qd_pop_p` move one, and nothing is counted for it. A `ptr` is never a struct, an array, a
string or a function value.

A native that returns a struct or an array pushes it with `qd_push_struct` or
`qd_push_array`, which hand the stack the reference the native holds.

A native that takes a function value declares the parameter with its `fn(...)` type. `qd_args`
hands the native the stack's reference to it; call it with `qd_call_value` as often as needed,
and release it with `qd_struct_release` when done:

```c
// each_up_to( n:i64 f:fn(i64 --) -- )
int each_up_to(qd_context* ctx) {
	int64_t n;
	void* fn;
	qd_args(ctx, "mathx::each_up_to", "ip", &n, &fn);
	for (int64_t i = 0; i < n; i++) {
		qd_push_i(ctx, i);       // the function's input
		qd_call_value(ctx, fn);
	}
	qd_struct_release(fn);
	return 0;
}
```

<!-- doccheck: skip links against an external .a built by the reader -->
```qd
import "libmath_ext.a" as "mathx" {
	pub fn each_up_to(n:i64 f:fn(i64 --) -- )
}

fn main() {
	3 fn (i64 -- ) { print nl } mathx::each_up_to  // 0 1 2, one per line
}
```

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

1. **Check return codes** - `qd_pop_*` returns non-zero on underflow or type mismatch; `qd_args` checks every argument before it pops any
2. **Use typed pop functions** - `qd_pop_i`, `qd_pop_f`, `qd_pop_s`, `qd_pop_p` handle type checking and string cleanup for you
3. **Use descriptive error messages** - they help debugging
4. **Keep functions small** - easier to test and maintain

## What's next?

Explore [Examples](../8-examples/file-processing.md) to see complete programs.
