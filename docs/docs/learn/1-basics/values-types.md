# Values and types

Every value in Quadrate has a type. The basic ones are numbers (`i64`, `f64`, `d128`) and
strings (`str`). On top of those come structs, arrays (`[]T`) and function values
(`fn(...)`), and `ptr` for raw memory.

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
	2.0 print nl      // prints 2; the literal is still an f64
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
	10 20 + -> sum
	$"Sum: {sum}" print nl         // Sum: 30
}
```

Each `{...}` names a variable or constant, and its value is converted to a string whatever its type. It is a name, not an expression: compute `x + y` into a local first. The `sb` module is auto-imported when `$"..."` is used.

### Choosing a string method

| Method | Best for |
|--------|----------|
| `$"Hello {name}"` | Formatting with embedded values |
| `"a" "b" strings::concat` | Simple two-string join |
| `sb::new` / `append` / `finish` | Building strings in loops or incrementally |

## Structs, arrays and function values

A struct value has its struct type, an array of `i64` has type `[]i64`, and a function value
has a `fn(...)` type that spells out its signature:

```qd
struct Point {
	x:f64
	y:f64
}

fn main() {
	Point {
		x = 1.0
		y = 2.0
	} -> p          // p is a Point
	p <<x print nl  // 1
	[1 2 3] -> xs   // xs is a []i64
	xs len print nl // 3
}
```

These get their own chapters: [Structs](../5-data-structures/structs.md),
[Arrays](../5-data-structures/arrays.md) and
[Function values](../7-advanced/function-pointers.md).

## Raw memory (ptr)

A `ptr` is a raw memory address: what `mem::alloc` returns and what C functions take and
return through [FFI](../7-advanced/ffi.md). It can be `null`. A `ptr` is not a struct, an
array, a string or a function value, and a parameter declared `ptr` refuses all of those.
Most programs never need one.

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
	"99" strconv::atoi ok if {
		-> n
		n print nl              // 99
	} else {
		drop
		"not a number" print nl
	}
}
```

## Type in function signatures

When you write functions, you specify types:

```qd
fn sum(a:i64 b:i64 -- result:i64) {
	+
}

def greet(name:str -- ) {
	"Hello, " print name print nl
}
```

The signature `(a:i64 b:i64 -- result:i64)` means:

- Two inputs: integers `a` and `b`
- One output: integer `result`

## What's next?

Now let's learn about [Operators](operators.md) to do things with these values.
