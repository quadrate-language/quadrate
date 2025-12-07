# Values and Types

Quadrate has four basic types. Understanding them is essential.

## Integer (i64)

64-bit signed integers:

```qd
fn main( -- ) {
	42 print nl
	-17 print nl
	1000000 print nl
}
```

## Float (f64)

64-bit floating-point numbers:

```qd
fn main( -- ) {
	3.14 print nl
	-0.5 print nl
	2.0 print nl      // Note: 2.0, not 2
}
```

**Important:** Write `2.0` not `2` when you want a float. `2` is an integer.

## String (str)

Text enclosed in double quotes:

```qd
fn main( -- ) {
	"Hello" print nl
	"Hello, World!" print nl
	"Line 1\nLine 2" print nl  // \n is newline
}
```

### Escape Sequences

| Escape | Meaning |
|--------|---------|
| `\n` | Newline |
| `\t` | Tab |
| `\\` | Backslash |
| `\"` | Double quote |

## Pointer (ptr)

Pointers reference data in memory. You'll use them with structs and arrays:

```qd
struct Point {
	x:f64
	y:f64
}

fn main( -- ) {
	Point {
		x = 1.0
		y = 2.0
	} -> p  // p is a ptr
	p @x print nl
}
```

We'll cover pointers in detail in the [Structs](../5-data-structures/structs.md) section.

## Booleans

Quadrate uses integers for boolean values:

- `true` = 1
- `false` = 0

```qd
fn main( -- ) {
	true print nl   // 1
	false print nl  // 0
}
```

Any non-zero value is considered true in conditionals.

## Type in Function Signatures

When you write functions, you specify types:

```qd
fn add(a:i64 b:i64 -- sum:i64) {
	+
}

fn greet(name:str -- ) {
	-> name
	"Hello, " print name print nl
}
```

The signature `(a:i64 b:i64 -- sum:i64)` means:

- Takes two integers (`a` and `b`)
- Returns one integer (`sum`)

## What's Next?

Now let's learn about [Operators](operators.md) to do things with these values.
