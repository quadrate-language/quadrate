# Anonymous functions

Anonymous functions (also called lambdas) let you define functions inline without naming them. They're useful for quick one-off operations and callbacks.

## Basic syntax

Use `fn (signature) { body }` to create an anonymous function:

```qd
fn main() {
	// Define and store an anonymous function
	fn (x:i64 -- r:i64) { 2 * } -> double

	// Call it with 'call'
	5 double call print nl  // 10
}
```

The signature follows the same format as regular functions: `(inputs -- outputs)`.

## Inline usage

Anonymous functions can be used directly without storing them:

```qd
fn main() {
	// Define and call immediately
	5 fn (x:i64 -- r:i64) { 2 * } call print nl  // 10

	// Multiple parameters
	10 20 fn (a:i64 b:i64 -- r:i64) { + } call print nl  // 30
}
```

## Side-effect functions

Functions that don't return values use an empty output signature:

```qd
fn main() {
	fn () { "Hello!" print nl } -> greet
	greet call  // Hello!

	// Or inline
	fn () { "Goodbye!" print nl } call  // Goodbye!
}
```

## Reusing anonymous functions

Once stored, an anonymous function can be called multiple times:

```qd
fn main() {
	fn (x:i64 -- r:i64) { 2 * } -> double

	// Use in a loop
	1 6 1 for i {
		i double call print nl
	}
	// Output: 2 4 6 8 10
}
```

## Control flow inside

Anonymous functions can contain conditionals and other control flow:

```qd
fn main() {
	fn (x:i64 -- r:i64) {
		dup 0 > if {
			10
		} else {
			0
		}
	} -> positive_to_ten

	5 positive_to_ten call print nl   // 10
	-3 positive_to_ten call print nl  // 0
}
```

## Closures (variable capture)

Anonymous functions can capture variables from their enclosing scope, creating closures:

```qd
fn main() {
	10 -> multiplier

	// This closure captures 'multiplier' from the outer scope
	fn (x:i64 -- r:i64) { multiplier * } -> scale

	5 scale call print nl   // 50
	7 scale call print nl   // 70
}
```

### Capture by reference

Variables are captured by reference. Changes to the original variable are visible to the closure:

```qd
fn main() {
	10 -> x

	fn ( -- r:i64) { x } -> get_x

	get_x call print nl  // 10

	99 -> x  // Change the original variable
	get_x call print nl  // 99 (sees the change)
}
```

### Multiple captures

Closures can capture multiple variables:

```qd
fn main() {
	10 -> a
	20 -> b
	30 -> c

	fn ( -- r:i64) { a b add c add } -> sum_all

	sum_all call print nl  // 60
}
```

### Closures in loops

Closures created inside loops capture the current value at each iteration:

```qd
fn main() {
	0 5 1 for i {
		i -> val
		fn ( -- r:i64) { val } -> c
		c call print nl
	}
	// Output: 0 1 2 3 4
}
```

### Nested closures

Closures can be nested, with inner closures capturing variables from outer closures:

```qd
fn main() {
	10 -> x

	fn ( -- r:i64) {
		x -> y
		fn ( -- r:i64) { y } -> inner
		inner call
	} -> outer

	outer call print nl  // 10
}
```

### Returning closures

Functions can return closures that capture their local variables:

```qd
fn make_adder(n:i64 -- adder:ptr) {
	-> amount  // Store parameter in local variable
	fn (x:i64 -- r:i64) { amount add }
}

fn main() {
	5 make_adder -> add5
	10 make_adder -> add10

	100 add5 call print nl   // 105
	100 add10 call print nl  // 110
}
```

## Calling named functions

Anonymous functions can call regular named functions:

```qd
fn helper(a:i64 -- r:i64) {
	10 +
}

fn main() {
	fn (a:i64 -- r:i64) { helper 2 * } -> process
	5 process call print nl  // 30 (5+10=15, 15*2=30)
}
```

## Comparison with function pointers

Anonymous functions and function pointers both use `call`:

```qd
// Named function with pointer
fn double(x:i64 -- r:i64) { 2 * }

fn main() {
	// Function pointer to named function
	&double -> fp1
	5 fp1 call print nl  // 10

	// Anonymous function
	fn (x:i64 -- r:i64) { 2 * } -> fp2
	5 fp2 call print nl  // 10
}
```

The difference is that anonymous functions are defined inline, while function pointers reference separately defined functions.

## What's next?

Learn about [Higher-Order Functions](higher-order-functions.md) for passing functions as arguments.
