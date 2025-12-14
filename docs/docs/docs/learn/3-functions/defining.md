# Defining Functions

Functions are the building blocks of Quadrate programs. They transform values on the stack.

## Basic Syntax

```qd
fn name(inputs -- outputs) {
	// body
}
```

Every function has:

- A **name**
- A **signature** describing stack effects
- A **body** with the implementation

## Your First Function

```qd
fn square(x:i64 -- result:i64) {
	dup *
}

fn main() {
	5 square print nl  // 25
}
```

The signature `(x:i64 -- result:i64)` means:

- **Input**: Takes one integer `x` from the stack
- **Output**: Leaves one integer `result` on the stack

## Stack Effect Signature

The `--` separates inputs from outputs:

```qd
fn add(a:i64 b:i64 -- sum:i64) {
	+
}

fn swap_print(a:i64 b:i64 -- ) {
	swap print nl print nl
}

fn push_two( -- a:i64 b:i64) {
	1 2
}
```

| Signature | Meaning |
|-----------|---------|
| `(a:i64 -- b:i64)` | 1 input, 1 output |
| `(a:i64 b:i64 -- c:i64)` | 2 inputs, 1 output |
| `(x:i64 -- )` | 1 input, no outputs |
| `( -- x:i64)` | No inputs, 1 output |
| `()` | No inputs, no outputs |

## Parameter Names

Parameter names document the values but don't create variables automatically:

```qd
fn distance(x1:f64 y1:f64 x2:f64 y2:f64 -- d:f64) {
	// Parameters are on the stack, not in variables
	// Use -> to bind them to names
	-> y2 -> x2 -> y1 -> x1

	x2 x1 - dup *
	y2 y1 - dup *
	+ sqrt
}
```

## Multiple Outputs

Functions can have multiple outputs:

```qd
fn divmod(a:i64 b:i64 -- quotient:i64 remainder:i64) {
	-> b -> a
	a b /
	a b %
}

fn main() {
	17 5 divmod
	print nl  // 2 (remainder)
	print nl  // 3 (quotient)
}
```

## Functions Without Parameters

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

## The main Function

Every program needs a `main` function:

```qd
fn main() {
	// Program starts here
}
```

`main` takes no inputs and has no outputs.

## Function Names

Rules for function names:

- Start with a letter or underscore
- Can contain letters, numbers, underscores
- Case-sensitive (`foo` and `Foo` are different)

```qd
fn calculate_total() { }
fn _private_helper() { }
fn processItem2() { }
```

## Type Annotations

Always specify types for parameters:

| Type | Description |
|------|-------------|
| `i64` | 64-bit integer |
| `f64` | 64-bit float |
| `str` | String |
| `ptr` | Pointer |
| `bool` | Boolean (alias for i64) |

```qd
fn format_price(price:f64 currency:str -- formatted:str) {
	-> currency -> price
	// ... implementation
}
```

## Documentation Comments

Use `///` for documentation:

```qd
/// Calculates the factorial of n
/// @param n The number to calculate factorial of
/// @output The factorial result
fn factorial(n:i64 -- result:i64) {
	-> n
	n 1 <= if {
		1
	} else {
		n n 1 - factorial *
	}
}
```

## What's Next?

Now let's learn how to [Call Functions](calling.md) and chain them together.
