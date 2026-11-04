# Language reference

This section documents all Quadrate keywords and built-in instructions.

## Quick navigation

- [Keywords](keywords.md) - Language keywords like `fn`, `if`, `for`, `struct`
- [Stack Operations](stack.md) - `dup`, `drop`, `swap`, `rot`, etc.
- [Arithmetic](arithmetic.md) - `+`, `-`, `*`, `/`, `%`
- [Comparison](comparison.md) - `==`, `!=`, `<`, `>`, `<=`, `>=`
- [Logic and bits](bitwise.md) - `and`, `or`, `not`, and `bits::` for bitwise and shifts
- [Arrays](arrays.md) - Array creation and manipulation
- [Structs](structs.md) - Struct definition, field access, methods
- [Generics](generics.md) - Generic functions with `<T>`
- [Type Casting](types.md) - `cast<T>`
- [Input/Output](io.md) - `print`, `dump`, `nl`
- [Error Handling](errors.md) - `panic`
- [Threading](threading.md) - `thread::spawn`, `join`, `detach`
- [Miscellaneous](misc.md) - `call`

## Types

Quadrate has five basic types:

| Type | Description | Example |
|------|-------------|---------|
| `i64` | 64-bit signed integer | `42`, `-17` |
| `f64` | 64-bit floating-point | `3.14`, `-0.5` |
| `d128` | Decimal floating-point, 34 digits | `0.1d`, `19.99d` |
| `str` | String | `"hello"` |
| `ptr` | Raw memory | `mem::alloc`, C handles |

## Stack signatures

Function signatures use `(inputs -- outputs)` notation:

```qd
fn sum(a:i64 b:i64 -- s:i64) { + }
```

- **Before `--`**: Values consumed from stack (bottom to top)
- **After `--`**: Values produced on stack (bottom to top)

## Fallible functions

Functions that can fail are marked with `!`:

<!-- doccheck: skip signature shown with its body elided -->
```qd
fn divide(a:i64 b:i64 -- result:i64)! {
	// Can signal error
}
```

Test the status with `ok if` (or `fail if`, or a `switch` on it) to handle errors:

<!-- doccheck: skip divide is shown above with its body elided -->
```qd
10 2 divide ok if {
	// success: the result is on the stack
	print nl
} else {
	// failure: a zero placeholder stands where the result would be
	drop
}
```
