# Defining functions

Functions are the building blocks of Quadrate programs. They transform values on the stack.

## Basic syntax

```qd
fn name(inputs -- outputs) {
	// body
}
```

Every function has:

- A **name**
- A **signature** describing stack effects
- A **body** with the implementation

## Your first function

```qd
fn square(x:i64 -- result:i64) {
	x x *
}

fn main() {
	5 square print nl  // 25
}
```

The signature `(x:i64 -- result:i64)` means:

- **Input**: Takes one integer `x` from the stack
- **Output**: Leaves one integer `result` on the stack

## Stack effect signature

The `--` separates inputs from outputs:

```qd
fn sum(a:i64 b:i64 -- result:i64) {
	a b +
}

fn swap_print(a:i64 b:i64 -- ) {
	b print nl a print nl
}

fn push_two( -- a:i64 b:i64) {
	1 2
}
```

| Signature | Meaning |
|-----------|---------|
| `(a:i64 -- b:i64)` | 1 input, 1 output |
| `(a:i64 b:i64 -- c:i64)` | 2 inputs, 1 output |
| `(x:i64 --)` | 1 input, no outputs |
| `(-- x:i64)` | No inputs, 1 output |
| `()` | No inputs, no outputs |

## Parameter names

Named input parameters are automatically bound as local variables:

```qd
use math

fn distance(x1:f64 y1:f64 x2:f64 y2:f64 -- d:f64) {
	x2 x1 - dup *  // (x2-x1)^2
	y2 y1 - dup *  // (y2-y1)^2
	+ math::sqrt
}
```

Simply use the parameter names to refer to their values:

```qd
fn add(a:i64 b:i64 -- sum:i64) {
	a b +
}
```

## Unnamed parameters

Parameters can omit names — they stay on the stack for direct manipulation:

```qd
fn double(i64 -- i64) {
	2 *       // operates directly on the stack value
}

fn add(i64 i64 -- i64) {
	+         // adds the two values on the stack
}
```

**When to use which:**
- **Named** (`a:i64`) — when you reference the value multiple times or the meaning isn't obvious
- **Unnamed** (`i64`) — for simple stack operations where the body is a single expression

The same applies to return types: name them for clarity in public APIs (`-- result:i64`), leave unnamed in lambdas and trivial functions (`-- i64`).

## Multiple outputs

Functions can have multiple outputs:

```qd
fn divmod(a:i64 b:i64 -- quotient:i64 remainder:i64) {
	a b /      // quotient
	a b %      // remainder
}

fn main() {
	17 5 divmod
	print nl  // 2 (remainder)
	print nl  // 3 (quotient)
}
```

## Functions without parameters

```qd
fn greet() {
	"Hello, World!" print nl
}

fn get_answer( -- answer:i64) {
	42
}

fn main() {
	greet
	get_answer print nl  // 42
}
```

## The main function

Every program needs a `main` function:

```qd
fn main() {
	// Program starts here
}
```

`main` takes no inputs and has no outputs.

## Function names

Rules for function names:

- Start with a letter or underscore
- Can contain letters, numbers, underscores
- Case-sensitive (`foo` and `Foo` are different)

```qd
fn calculate_total() { }
fn processItem2() { }
```

## Type annotations

Always specify types for parameters:

| Type | Description |
|------|-------------|
| `i64` | 64-bit integer |
| `f64` | 64-bit float |
| `str` | String |
| `ptr` | Pointer |
| `any` | Any type |

Booleans are represented as `i64` (0 = false, non-zero = true).

```qd
fn format_price(price:f64 currency:str -- formatted:str) {
	// ... implementation using price and currency
}
```

## Documentation comments

Use `///` for documentation:

```qd
/// Calculates the factorial of n
/// @param n The number to calculate factorial of
/// <<output The factorial result
fn factorial(n:i64 -- result:i64) {
	n 1 <= if {
		1
	} else {
		n n 1 - factorial *
	}
}
```

## Inline functions

The `inline` keyword requests the compiler to inline the function body at call sites, eliminating function call overhead. The compiler will inline in most cases, but may decline for recursive functions or indirect calls.

```qd
inline fn double(x:i64 -- result:i64) {
	x 2 *
}
```

Can be combined with `pub`:

```qd
pub inline fn inc(x:i64 -- result:i64) {
	x 1 +
}
```

Use `inline` for small helper functions where the call overhead would dominate the actual work. The standard library's `sys` module uses this for zero-overhead hardware access wrappers.

## What's next?

Now let's learn how to [Call Functions](calling.md) and chain them together.
