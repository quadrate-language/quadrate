# Calling Functions

In Quadrate, you call functions by simply writing their name.

## Basic Calls

```qd
fn double(x:i64 -- result:i64) {
	2 *
}

fn main( -- ) {
	5 double print nl  // 10
}
```

No parentheses needed! Just push the arguments, then write the function name.

## Chaining Functions

Functions naturally chain together:

```qd
fn double(x:i64 -- result:i64) {
	2 *
}

fn square(x:i64 -- result:i64) {
	dup *
}

fn add_one(x:i64 -- result:i64) {
	1 +
}

fn main( -- ) {
	3 double square add_one print nl
	// 3 -> 6 -> 36 -> 37
}
```

Each function consumes its inputs and produces outputs for the next.

## Multiple Arguments

Push all arguments before calling:

```qd
fn add3(a:i64 b:i64 c:i64 -- sum:i64) {
	+ +
}

fn main( -- ) {
	1 2 3 add3 print nl  // 6
}
```

Stack before call: `[1, 2, 3]`
Stack after call: `[6]`

## Multiple Outputs

Capture multiple outputs:

```qd
fn minmax(a:i64 b:i64 -- min:i64 max:i64) {
	-> b -> a
	a b < if {
		a b
	} else {
		b a
	}
}

fn main( -- ) {
	10 3 minmax
	// Stack: [3, 10]
	print nl  // 10 (max)
	print nl  // 3 (min)
}
```

## Using Outputs

You can immediately use outputs:

```qd
fn square(x:i64 -- result:i64) {
	dup *
}

fn main( -- ) {
	// Use result in expression
	3 square 4 square + print nl  // 9 + 16 = 25

	// Use result multiple times
	5 square dup print nl print nl  // 25, 25

	// Store result
	7 square -> sqd
	sqd print nl  // 49
}
```

## Calling Library Functions

Library functions work the same way:

```qd
use math

fn main( -- ) {
	3.14159 math::sin print nl  // sine
	2.0 math::sqrt print nl     // square root
	-5 abs print nl             // absolute value
}
```

## Recursive Calls

Functions can call themselves:

```qd
fn factorial(n:i64 -- result:i64) {
	-> n
	n 1 <= if {
		1
	} else {
		n n 1 - factorial *
	}
}

fn main( -- ) {
	5 factorial print nl  // 120
}
```

## Mutual Recursion

Functions can call each other:

```qd
fn is_even(n:i64 -- result:i64) {
	-> n
	n 0 == if {
		1
	} else {
		n 1 - is_odd
	}
}

fn is_odd(n:i64 -- result:i64) {
	-> n
	n 0 == if {
		0
	} else {
		n 1 - is_even
	}
}

fn main( -- ) {
	4 is_even print nl  // 1 (true)
	4 is_odd print nl   // 0 (false)
}
```

## Order of Evaluation

Operations execute left to right:

```qd
fn main( -- ) {
	// This:
	2 3 + 4 *

	// Means:
	// 1. Push 2
	// 2. Push 3
	// 3. Add (2+3=5)
	// 4. Push 4
	// 5. Multiply (5*4=20)

	print nl  // 20
}
```

## Nested Calls

Functions can call other functions:

```qd
fn inner(x:i64 -- result:i64) {
	1 +
}

fn outer(x:i64 -- result:i64) {
	inner 10 *
}

fn main( -- ) {
	5 outer print nl  // (5+1)*10 = 60
}
```

## What's Next?

Learn how to organize code into [Modules](modules.md) for larger projects.
