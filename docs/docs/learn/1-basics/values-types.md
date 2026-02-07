# Values and types

Quadrate has four basic types. Understanding them is essential.

## Integer (i64)

64-bit signed integers:

```qd
fn main() {
	42 print nl
	-17 print nl
	1000000 print nl
}
```

### Numeric literals

Integers can be written in decimal, hexadecimal, or binary:

```qd
fn main() {
	255 print nl       // decimal
	0xFF print nl      // hexadecimal (255)
	0b11111111 print nl  // binary (255)
}
```

| Prefix | Base | Example |
|--------|------|---------|
| (none) | Decimal | `42`, `-17` |
| `0x` | Hexadecimal | `0xFF`, `0x1A` |
| `0b` | Binary | `0b1010`, `0b11110000` |

## Float (f64)

64-bit floating-point numbers:

```qd
fn main() {
	3.14 print nl
	-0.5 print nl
	2.0 print nl      // Note: 2.0, not 2
}
```

**Important:** Write `2.0` not `2` when you want a float. `2` is an integer.

## String (str)

Text enclosed in double quotes:

```qd
fn main() {
	"Hello" print nl
	"Hello, World!" print nl
	"Line 1\nLine 2" print nl  // \n is newline
}
```

### Escape sequences

| Escape | Meaning |
|--------|---------|
| `\n` | Newline |
| `\r` | Carriage return |
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

fn main() {
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
fn main() {
	true print nl   // 1
	false print nl  // 0
}
```

Any non-zero value is considered true in conditionals.

## Converting between types

Use `cast<T>` to convert a value to a different type:

```qd
fn main() {
	42 cast<f64> print nl    // 42
	3.14 cast<i64> print nl  // 3 (truncates)
	42 cast<str> print nl    // 42
	"99" cast<i64> print nl  // 99
}
```

This is especially useful when mixing integer and float operations.

## Type in function signatures

When you write functions, you specify types:

```qd
fn sum(a:i64 b:i64 -- result:i64) {
	+
}

fn greet(name:str -- ) {
	-> name  // bind parameter
	"Hello, " print name print nl
}
```

The signature `(a:i64 b:i64 -- result:i64)` means:

- Two inputs: integers `a` and `b`
- One output: integer `result`

## What's next?

Now let's learn about [Operators](operators.md) to do things with these values.
