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
	dup *
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
| `(x:i64 --)` | 1 input, no outputs |
| `(-- x:i64)` | No inputs, 1 output |
| `()` | No inputs, no outputs |

## Parameter names

Parameter names document the values but don't create variables automatically. You must use `->` to bind them:

```qd
use math

fn distance(x1:f64 y1:f64 x2:f64 y2:f64 -- d:f64) {
	-> y2 -> x2 -> y1 -> x1  // bind parameters to variables
	x2 x1 - dup *  // (x2-x1)^2
	y2 y1 - dup *  // (y2-y1)^2
	+ math::sqrt
}
```

!!! warning "Common mistake"
    Parameter names like `a` and `b` in `fn add(a:i64 b:i64 -- sum:i64)` are only for documentation. They do **not** create variables you can use in the body. This won't work:

    ```qd
    fn add(a:i64 b:i64 -- sum:i64) {
    	a b +  // ERROR: 'a' and 'b' are not defined
    }
    ```

    Instead, either use stack operations or bind with `->`:

    ```qd
    fn add(a:i64 b:i64 -- sum:i64) {
    	+  // Works: operates directly on the stack
    }

    fn add(a:i64 b:i64 -- sum:i64) {
    	-> b -> a  // Works: bind to variables first
    	a b +
    }
    ```

## Multiple outputs

Functions can have multiple outputs:

```qd
fn divmod(a:i64 b:i64 -- quotient:i64 remainder:i64) {
	-> b -> a  // bind parameters
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
| `bool` | Boolean (alias for i64) |

```qd
fn format_price(price:f64 currency:str -- formatted:str) {
	-> currency -> price
	// ... implementation
}
```

## Documentation comments

Use `///` for documentation:

```qd
/// Calculates the factorial of n
/// @param n The number to calculate factorial of
/// @output The factorial result
fn factorial(n:i64 -- result:i64) {
	-> n  // bind parameter
	n 1 <= if {
		1
	} else {
		n n 1 - factorial *
	}
}
```

## What's next?

Now let's learn how to [Call Functions](calling.md) and chain them together.
