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

## The same rules as a named function

An anonymous function is a function that happens to have no name. Everything a named function's
signature means, its signature means too.

**The inputs stay on the stack.** The body starts with its arguments where the caller left them,
and the names are documentation, so they are optional:

```qd
fn main() {
	3 4 fn (i64 i64 -- r:i64) { + } call print nl  // 7
}
```

**`def` binds them.** With `def`, each input is taken off the stack at entry and bound as a
local, so the body says `x 2 *` and not `2 *`:

```qd
fn main() {
	5 def (x:i64 -- r:i64) { x 2 * } call print nl  // 10
}
```

In a `def`, every input must be named — an unnamed one is an error that tells you to name it or
to write `fn`. A bare type name in the parameter list is a *type*, never a parameter name:
`fn (Point -- r:f64)` takes a `Point`.

**The body must leave what the signature declares**, in the number and the types it declares. A
body that leaves two values under `-- r:i64` does not compile.

**`!` makes it fallible.** Only then may the body `panic`, and the call site has to say what
happens on failure — `call!` to abort, `call?` to propagate, or a bare `call` read by `if` or
`switch`:

```qd
fn main() {
	0 def (x:i64 -- r:i64)! { x 0 == if { "divide by zero" 1 panic } 100 x / } call ok if {
		print nl
	} else {
		drop
		"caught" print nl  // caught
	}
}
```

## Without a signature

`fn { body }` has no signature at all. The body works on the stack, as any function's does, and
the compiler works out what it takes and leaves from what the body does:

```qd
fn main() {
	5 fn { 2 * } call print nl                // 10
	3 4 fn { + } call print nl                // 7
	10 20 fn { 1 + } dip print " " print print nl   // 20 11
	[1 2 3] fn { dup * } map 2 nth print nl         // 9
}
```

`fn { 2 * }` takes one value and leaves one; `fn { + }` takes two. Its effect is checked
wherever it goes exactly as a written signature is, so passing `fn { + }` to `map`, which
runs its function value on one value at a time, is a compile error.

The effect is worked out from the *counts*; the types are left open where the body does not fix
them. Editors show the inferred effect next to the `fn` — `fn (_ -- _) { 2 * }` — with `_` for a
type the body leaves open. Write a signature when the types matter to a reader, when the body
binds names, or when the function value must be fallible: a `fn { }` cannot be.

## Inline usage

Anonymous functions can be used directly without storing them:

```qd
fn main() {
	// Define and call immediately
	5 def (x:i64 -- r:i64) { x 2 * } call print nl  // 10

	// Multiple parameters
	10 20 def (a:i64 b:i64 -- r:i64) { a b + } call print nl  // 30
}
```

## As a value in a literal

An anonymous function is a value, so it can initialise a struct field or be an element of an array
literal — no need to bind it to a local first:

```qd
struct Ops {
	add:fn(i64 -- i64)
	name:str
}

fn main() {
	Ops { add = def (x:i64 -- r:i64) { x 1 + }  name = "ops" } -> o
	10 o <<add call print nl  // 11

	[def (x:i64 -- r:i64) { x 2 * }] -> fns
	5 fns 0 nth call print nl  // 10
	fns free
}
```

Binding it first still works and reads better when the same function is used more than once:

```qd
struct Ops {
	add:fn(i64 -- i64)
	mul:fn(i64 -- i64)
}

fn main() {
	def (x:i64 -- r:i64) { x 2 * } -> double
	Ops { add = double  mul = double } -> o
	5 o <<add call print nl  // 10
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
	def (x:i64 -- r:i64) { x 2 * } -> double

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
	def (x:i64 -- r:i64) {
		x 0 > if {
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
	def (x:i64 -- r:i64) { x multiplier * } -> scale

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

	fn ( -- r:i64) { a b + c + } -> sum_all

	sum_all call print nl  // 60
}
```

### Closures in loops

A `for` iterator can be captured directly, and it is captured **by value**: each closure keeps the
iterator as it stood when the closure was built, so one that outlives the loop still reports its own
iteration.

```qd
fn main() {
	0 5 1 for i {
		fn ( -- r:i64) { i } -> c
		c call print nl
	}
	// Output: 0 1 2 3 4
}
```

A local declared in the loop body behaves the same way: each iteration gives it a fresh binding, so
a closure that escapes reports the value from its own pass.

```qd
fn main() {
	[] -> fns
	0 3 1 for i {
		i 10 * -> val
		fns fn ( -- r:i64) { val } append -> fns
	}
	0 3 1 for n {
		fns n nth call print nl
	}
	// Output: 0 10 20
	fns free
}
```

Capture by reference (above) still applies **within** a binding: reassigning a variable is visible
through a closure that captured it. What a new iteration produces is a new binding, not a new value
in the old one — the same distinction `let` draws from `var` in other languages.

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
fn make_adder(i64 -- adder:fn(i64 -- i64)) {
	-> n
	def (x:i64 -- r:i64) { x n + }
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
def helper(a:i64 -- r:i64) {
	a 10 +
}

fn main() {
	def (a:i64 -- r:i64) { a helper 2 * } -> process
	5 process call print nl  // 30 (5+10=15, 15*2=30)
}
```

## Comparison with function pointers

Anonymous functions and function pointers both use `call`:

```qd
// Named function with pointer
def double(x:i64 -- r:i64) { x 2 * }

fn main() {
	// Function pointer to named function
	&double -> fp1
	5 fp1 call print nl  // 10

	// Anonymous function
	def (x:i64 -- r:i64) { x 2 * } -> fp2
	5 fp2 call print nl  // 10
}
```

The difference is that anonymous functions are defined inline, while function pointers reference separately defined functions.

## What's next?

Learn about [Higher-Order Functions](higher-order-functions.md) for passing functions as arguments.
