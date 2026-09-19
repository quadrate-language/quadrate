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

## dup2 - duplicate a pair

`dup2` is the one two-element variant, duplicating the top pair:

```qd
fn main() {
	1 2 dup2 // [1, 2] -> [1, 2, 1, 2]
	print nl // 2
	print nl // 1
	print nl // 2
	print nl // 1
}
```

## Deep stack access

### pick - copy the third value

`pick` reaches one deeper than `over`:

```qd
fn main() {
	10 20 30
	pick       // ( x y z -- x y z x )
	print nl   // 10
	drop drop drop
}
```

That is as deep as the stack words reach, on purpose. `pick` used to take the depth as a value
(`2 pick`), and `roll` moved the nth value to the top the same way; both have been removed in that
form, because a depth that is only known at run time means nothing can say what the code around it
leaves on the stack.

Below three deep, name what you need:

```qd
fn main() {
	10 20 30 40
	-> d -> c -> b -> a
	a print nl   // 10
}
```

## Utility operations

### depth - stack size

```qd
fn main() {
	1 2 3
	depth print nl  // 3
	drop drop drop
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
| `dup2` | [a, b] | [a, b, a, b] | Duplicate pair |

## What's next?

Stack manipulation can get complex. Let's learn about [Local Variables](local-variables.md) for when you need to name values.
