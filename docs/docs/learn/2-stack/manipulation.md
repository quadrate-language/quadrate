# Stack manipulation

These operations rearrange values on the stack without changing them.

## dup - duplicate

Copy the top value:

```qd
fn main() {
	5 dup    // [5] -> [5, 5]
	print nl // 5
	print nl // 5
}
```

Use `dup` when you need to use a value twice:

```qd
fn main() {
	5 dup * print nl  // 5 * 5 = 25
}
```

## drop - discard

Remove the top value:

```qd
fn main() {
	1 2 3    // [1, 2, 3]
	drop     // [1, 2]
	print nl // 2
	print nl // 1
}
```

Use `drop` to discard values you don't need.

## swap - exchange

Swap the top two values:

```qd
fn main() {
	1 2      // [1, 2]
	swap     // [2, 1]
	print nl // 1
	print nl // 2
}
```

Use `swap` when values are in the wrong order:

```qd
fn main() {
	3 10     // Want 10 - 3, but 3 is below 10
	swap     // [10, 3]
	- print nl // 7
}
```

## over - copy second

Copy the second value to the top:

```qd
fn main() {
	1 2      // [1, 2]
	over     // [1, 2, 1]
	print nl // 1
	print nl // 2
	print nl // 1
}
```

## rot - rotate three

Rotate the top three values (third moves to top):

```qd
fn main() {
	1 2 3    // [1, 2, 3]
	rot      // [2, 3, 1]
	print nl // 1
	print nl // 3
	print nl // 2
}
```

## nip - remove second

Remove the second value:

```qd
fn main() {
	1 2      // [1, 2]
	nip      // [2]
	print nl // 2
}
```

`nip` is equivalent to `swap drop`.

## tuck - copy under

Copy the top value under the second:

```qd
fn main() {
	1 2      // [1, 2]
	tuck     // [2, 1, 2]
	print nl // 2
	print nl // 1
	print nl // 2
}
```

## Two-element variants

Operations with `2` suffix work on pairs:

```qd
fn main() {
	1 2 dup2   // [1, 2] -> [1, 2, 1, 2]
	1 2 drop2  // [1, 2] -> []
	1 2 3 4 swap2  // [1, 2, 3, 4] -> [3, 4, 1, 2]
}
```

## Deep stack access

For values deeper in the stack:

### pick - copy nth value

```qd
fn main() {
	10 20 30 40
	2 pick     // Copy index 2 (value 20)
	print nl   // 20
}
```

Index 0 is the top, 1 is second, etc.

### roll - move nth value

```qd
fn main() {
	10 20 30 40
	2 roll     // Move index 2 to top (0-based: 0=top)
	print nl   // 20
	// Stack now: [10, 30, 40]
}
```

Index 0 is the top, 1 is second, etc. (same as `pick`).

## Utility operations

### depth - stack size

```qd
fn main() {
	1 2 3
	depth print nl  // 3
}
```

### clear - empty stack

```qd
fn main() {
	1 2 3 4 5
	clear
	depth print nl  // 0
}
```

## Quick reference

| Op | Before | After | Description |
|----|--------|-------|-------------|
| `dup` | [a] | [a, a] | Duplicate top |
| `drop` | [a] | [] | Remove top |
| `swap` | [a, b] | [b, a] | Swap top two |
| `over` | [a, b] | [a, b, a] | Copy second |
| `rot` | [a, b, c] | [b, c, a] | Rotate three |
| `nip` | [a, b] | [b] | Remove second |
| `tuck` | [a, b] | [b, a, b] | Copy top under |

## What's next?

Stack manipulation can get complex. Let's learn about [Local Variables](local-variables.md) for when you need to name values.
