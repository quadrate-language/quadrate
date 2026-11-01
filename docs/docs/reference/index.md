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
- [Type Casting](types.md) - `cast<T>`, `as TypeName`
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
| `ptr` | Pointer | Struct instances, arrays |

## Stack signatures

Function signatures use `(inputs -- outputs)` notation:

```qd
def sum(a:i64 b:i64 -- s:i64) { a b + }
```

- **Before `--`**: Values consumed from stack (bottom to top)
- **After `--`**: Values produced on stack (bottom to top)

## Fallible functions

Functions that can fail are marked with `!`:

<!-- doccheck: skip signature shown with its body elided -->
```qd
def divide(a:i64 b:i64 -- result:i64)! {
	// Can signal error
}
```

Call with `if-else` to handle errors:

```qd
10 2 divide if {
	// success: result is on stack
} else {
	// error: stack is clean (inputs consumed, no outputs)
}
```
