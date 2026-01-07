# Local variables

When stack manipulation gets complex, use local variables.

## The `->` operator

Pop a value and store it in a named variable:

```qd
fn main() {
	42 -> x     // Pop 42, store in x
	x print nl  // Push x, print it
}
```

## Why use variables?

Compare these two approaches:

**Stack manipulation only:**

```qd
fn distance(x1:f64 y1:f64 x2:f64 y2:f64 -- d:f64) {
	rot - dup *   // (y2-y1)^2
	rot rot - dup * // (x2-x1)^2
	+ sqrt
}
```

**With local variables:**

```qd
fn distance(x1:f64 y1:f64 x2:f64 y2:f64 -- d:f64) {
	-> y2 -> x2 -> y1 -> x1
	x2 x1 - dup *
	y2 y1 - dup *
	+ sqrt
}
```

The second version is much clearer!

## Storing multiple values

You can store multiple values:

```qd
fn main() {
	1 2 3
	-> c -> b -> a  // a=1, b=2, c=3
	a print nl
	b print nl
	c print nl
}
```

**Note:** Values are popped in reverse order. The top of stack (`3`) goes into the first variable (`c`).

## Variables in expressions

Use variables like any other value:

```qd
fn main() {
	5 -> x
	10 -> y

	x y + print nl      // 15
	x y * print nl      // 50
	x x * print nl      // 25 (x squared)
}
```

## Reassigning variables

You can update a variable:

```qd
fn main() {
	0 -> count

	count print nl      // 0
	count 1 + -> count  // increment
	count print nl      // 1
	count 1 + -> count
	count print nl      // 2
}
```

## Scope

Variables are scoped to their function:

```qd
fn foo() {
	42 -> x
	x print nl  // Works: x is in scope
}

fn bar() {
	// x is NOT available here
}
```

## Block scope

Variables defined inside blocks (`if`, `for`, `loop`, `switch`) are only visible within that block:

```qd
fn main() {
	true if {
		42 -> x
		x print nl  // Works: x is in scope
	}
	// x is NOT available here - it's out of scope
}
```

If you need a variable after the block, define it before:

```qd
fn main() {
	0 -> result
	true if {
		42 -> result
	}
	result print nl  // Works: result was defined outside the block
}
```

## Common patterns

### Swap with variables

```qd
fn main() {
	1 2          // [1, 2]
	-> b -> a    // a=1, b=2
	b a          // [2, 1]
}
```

### Using a value multiple times

```qd
fn main() {
	42 -> x
	x print nl
	x x * print nl
	x x x * * print nl
}
```

### Computing intermediate results

```qd
fn quadratic(a:f64 b:f64 c:f64 x:f64 -- result:f64) {
	-> x -> c -> b -> a

	// ax^2 + bx + c
	a x dup * *    // ax^2
	-> ax2

	b x *          // bx
	-> bx

	ax2 bx + c +   // ax^2 + bx + c
}
```

## When to use variables vs stack

**Use stack manipulation when:**

- Operations are simple
- Values are used once in sequence
- The code is short

**Use variables when:**

- Values are used multiple times
- Operations span many lines
- Clarity is more important than brevity

## What's next?

Now let's learn how to [Define Functions](../3-functions/defining.md).
