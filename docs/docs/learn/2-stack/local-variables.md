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

Compare these two approaches for computing `(x - y) * (x + y)`:

**Stack manipulation only:**

```qd
fn diff_of_squares(x:i64 y:i64 -- result:i64) {
	over over - rot rot + *
}
```

**With local variables:**

```qd
fn diff_of_squares(x:i64 y:i64 -- result:i64) {
	x y - x y + *
}
```

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
	1 -> a
	2 -> b
	b print nl  // 2
	a print nl  // 1
}
```

### Using a value multiple times

```qd
fn main() {
	42 -> x
	x print nl        // 42
	x x * print nl    // 1764
	x x x * * print nl  // 74088
}
```

### Computing intermediate results

```qd
fn quadratic(a:f64 b:f64 c:f64 x:f64 -- result:f64) {
	a x dup * * -> ax2   // ax^2
	b x * -> bx         // bx
	ax2 bx + c +        // ax^2 + bx + c
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

Now let's learn about [Values and Types](../1-basics/values-types.md).
