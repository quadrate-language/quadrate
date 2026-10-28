# Values and types

Quadrate has five basic types. Understanding them is essential.

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

## Decimal (d128)

A `d128` is a decimal number with 34 significant digits, written with a `d`. Where an `f64`
holds the nearest binary fraction, a `d128` holds the decimal digits exactly, so it is the
type for money and anything else people count in tenths and hundredths:

```qd
use decimal

fn main() {
	0.1 0.2 + 0.3 == print nl     // 0
	0.1d 0.2d + 0.3d == print nl  // 1
	19.99d 3 * print nl           // 59.97
	2.50d print nl                // 2.50
	10d 3d / 0.01d decimal::quantize print nl  // 3.33
}
```

An integer mixes with a `d128`, as `19.99d 3 *` does; an `f64` does not, and needs
`cast<d128>`. A value keeps the decimal places it was written or computed with, so `2.50d`
prints as `2.50`, and `decimal::quantize` rounds to a number of places. The
[`decimal`](../../stdlib/decimal.md) module has the rest: `sqrt`, `pow`, rounding and more.

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

### String interpolation

Use `$"..."` to embed expressions in strings:

```qd
fn main() {
	"World" -> name
	42 -> age
	$"Hello, {name}!" print nl     // Hello, World!
	$"Age: {age}" print nl         // Age: 42
	$"Sum: {age}" print nl         // Sum: 42
}
```

Each `{...}` names a variable or constant, and its value is converted to a string whatever its type. It is a name, not an expression: compute `x + y` into a local first. The `sb` module is auto-imported when `$"..."` is used.

### Choosing a string method

| Method | Best for |
|--------|----------|
| `$"Hello {name}"` | Formatting with embedded values |
| `"a" "b" strings::concat` | Simple two-string join |
| `sb::new` / `append` / `finish` | Building strings in loops or incrementally |

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
	p <<x print nl
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
}
```

This is especially useful when mixing integer and float operations.

`cast` only does conversions that always succeed. Turning a string into a number is not one
of them -- `"12x"` and `"hello"` have no numeric value -- so that direction lives in
`strconv`, which tells you whether it worked:

```qd
use strconv

fn main() {
	"99" strconv::atoi if {
		-> n
		n print nl              // 99
	} else {
		"not a number" print nl
	}
}
```

## Type in function signatures

When you write functions, you specify types:

```qd
fn sum(a:i64 b:i64 -- result:i64) {
	a b +
}

fn greet(name:str -- ) {
	"Hello, " print name print nl
}
```

The signature `(a:i64 b:i64 -- result:i64)` means:

- Two inputs: integers `a` and `b`
- One output: integer `result`

## What's next?

Now let's learn about [Operators](operators.md) to do things with these values.
