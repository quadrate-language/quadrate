# Common mistakes

Gotchas and frequent errors when learning Quadrate.

## Using parameter names as variables

Parameter names in function signatures are **documentation only**. They don't create variables:

```qd
// WRONG - a and b are not defined
fn add(a:i64 b:i64 -- sum:i64) {
	a b +  // Error!
}

// RIGHT - use stack operations
fn add(a:i64 b:i64 -- sum:i64) {
	+
}

// RIGHT - bind with ->
fn add(a:i64 b:i64 -- sum:i64) {
	-> b -> a
	a b +
}
```

## Wrong argument order

In a stack-based language, argument order matters. The last value pushed is on top:

```qd
// Push 10 then 3: stack is [10, 3] with 3 on top
// The - operator does: second_from_top - top = 10 - 3 = 7
10 3 - print nl  // 7, not -7

// If you want 3 - 10:
3 10 - print nl  // -7
```

## Forgetting -> when binding parameters

A common pattern is to forget binding parameters in reverse order:

```qd
// WRONG - parameters are in wrong order
fn distance(x1:f64 y1:f64 x2:f64 y2:f64 -- d:f64) {
	-> x1 -> y1 -> x2 -> y2  // x1 gets the LAST pushed value (y2)!
}

// RIGHT - bind in reverse order (last pushed = first popped)
fn distance(x1:f64 y1:f64 x2:f64 y2:f64 -- d:f64) {
	-> y2 -> x2 -> y1 -> x1  // Correct order
}
```

The last parameter in the signature is on top of the stack, so it gets popped first.

## Stack underflow

Trying to use more values than are on the stack:

```qd
// WRONG - only one value on stack, but + needs two
fn main() {
	5
	+  // Error: stack underflow
}
```

## Leaving values on the stack

Functions must leave exactly the number of values declared in their output signature:

```qd
// WRONG - leaves extra value on stack
fn double(x:i64 -- result:i64) {
	-> x
	x
	x 2 *  // Stack has [x, x*2] but signature says only 1 output
}

// RIGHT
fn double(x:i64 -- result:i64) {
	-> x
	x 2 *  // Stack has [x*2], matches 1 output
}
```

## Forgetting to handle errors

Fallible functions (marked with `!`) must have their errors handled:

```qd
use io

// WRONG - compiler error, must handle the error
fn main() {
	"test.txt" io::read_file
}

// RIGHT - handle with if/else
fn main() {
	"test.txt" io::read_file if {
		-> content
		content print
	} else {
		"Could not read file" print nl
	}
}

// RIGHT - abort on error with !
fn main() {
	"test.txt" io::read_file! -> content
	content print
}
```

## Module not found

If `use json` gives an "unknown module" error, the module might be an external package that needs to be installed:

```bash
quadpm get https://github.com/quadrate-language/json
```

See [External Packages](../external-packages.md) for the full list.

## Integer vs float confusion

`2` is an integer, `2.0` is a float. Mixing them causes type errors:

```qd
// WRONG - 2 is i64, but math::sqrt expects f64
2 math::sqrt

// RIGHT
2.0 math::sqrt
```

Use `cast` to convert between types:

```qd
2 cast<f64> math::sqrt print nl  // 1.414...
```
