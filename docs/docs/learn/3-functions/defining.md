# Defining functions

Functions are the building blocks of Quadrate programs. They transform values on the stack.

## Basic syntax

<!-- doccheck: skip syntax template: `inputs`/`outputs` stand for type lists, not types -->
```qd
def name(inputs -- outputs) {
	// body
}
```

Every function has:

- A **name**
- A **signature** describing stack effects
- A **body** with the implementation

## Your first function

```qd
def square(x:i64 -- result:i64) {
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
def sum(a:i64 b:i64 -- result:i64) {
	a b +
}

def swap_print(a:i64 b:i64 -- ) {
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

## Inputs stay on the stack

A function's inputs are on the stack when its body starts, where the caller put them, and the body
works on them directly. The names in the signature are documentation: they show up in editor
hints, generated docs and error messages, but they are not in scope.

```qd
fn double(x:i64 -- r:i64) {
	2 *       // the value is on the stack
}

fn sum(a:i64 b:i64 -- s:i64) {
	+
}
```

Names are optional, so a short word can say just the types: `fn double(i64 -- i64) { 2 * }`.

## `def` — binding the inputs to names

When a body would rather read its inputs by name, declare it with `def`. Each input is then taken off the
stack at entry and bound as a local of that name, and the body starts on an empty stack:

```qd
use math

def distance(x1:f64 y1:f64 x2:f64 y2:f64 -- d:f64) {
	x2 x1 - dup *  // (x2-x1)^2
	y2 y1 - dup *  // (y2-y1)^2
	+ math::sqrt
}
```

In a `def` every input must be named, since each one is bound. Naming an input in the body of a
`fn` is an error that says to write `def`.

**When to use which:**
- **Plain `fn`** — the default: the body is a stack expression over its inputs
- **`def`** — when the inputs are used several times or in an order the stack makes awkward,
  and names read better than shuffling

The same applies to return types: name them for clarity in public APIs (`-- result:i64`), leave unnamed in lambdas and trivial functions (`-- i64`).

## Multiple outputs

Functions can have multiple outputs:

```qd
def divmod(a:i64 b:i64 -- quotient:i64 remainder:i64) {
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

<!-- doccheck: skip signature shown with its body elided -->
```qd
def format_price(price:f64 currency:str -- formatted:str) {
	// ... implementation using price and currency
}
```

## Documentation comments

Use `///` for documentation:

```qd
/// Calculates the factorial of n
/// @param n The number to calculate factorial of
/// <<output The factorial result
def factorial(n:i64 -- result:i64) {
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
inline def double(x:i64 -- result:i64) {
	x 2 *
}
```

Can be combined with `pub`:

```qd
pub inline def incr(x:i64 -- result:i64) {
	x 1 +
}
```

Use `inline` for small helper functions where the call overhead would dominate the actual work. The standard library's `sys` module uses this for zero-overhead hardware access wrappers.

## What's next?

Now let's learn how to [Call Functions](calling.md) and chain them together.
