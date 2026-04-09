# Type casting

Operations for converting between types.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `cast<T>` | `(val -- T)` | Convert to specified type (runtime) |
| `as Type` | `(val -- val)` | Narrow ptr to struct type (compile-time) |

---

## Conversions

### cast

Converts a value to the specified type using the `cast<T>` syntax.

**Signature:** `(val -- T)`

**Examples:**

```qd
3.14 cast<i64>   // 3 (truncates toward zero)
42 cast<f64>     // 42
42 cast<str>     // "42"
"3.14" cast<f64> // 3.14 (parses string)
"42" cast<i64>   // 42 (parses string)
```

**Supported types:** `i64`, `f64`, `str`, `ptr`

---

## Type system

### Basic types

| Type | Description | Size |
|------|-------------|------|
| `i64` | 64-bit signed integer | 8 bytes |
| `f64` | 64-bit floating-point | 8 bytes |
| `str` | String | Variable |
| `ptr` | Pointer | 8 bytes |
| `[]T` | Typed array (e.g., `[]i64`, `[]f64`, `[]str`) | 8 bytes (pointer) |
| `fn(...)` | Typed function pointer (e.g., `fn(i64 -- i64)`) | 8 bytes (pointer) |
| `bool` | Boolean (alias for i64) | 8 bytes |

### Type declarations

In function signatures:

```qd
fn process(x:i64 y:f64 name:str data:[]i64 -- result:i64) {
	// ...
}
```

With function pointer types:

```qd
fn apply(x:i64 f:fn(i64 -- i64) -- result:i64) {
	x f call
}
```

In struct fields:

```qd
struct Matrix {
	data:[]f64
	rows:i64
	cols:i64
}
```

### Type aliases

The `type` keyword creates a name for an existing type:

```qd
type Transform = fn(i64 -- i64)
type IntList = []i64
```

Type aliases are interchangeable with their underlying type.

### Type checking

Types are checked at compile time. The compiler verifies stack effects match declared signatures.

### Implicit conversions

Quadrate does not perform implicit type conversions. Use explicit casts:

```qd
// WRONG: Type mismatch
5 3.0 + // Error!

// CORRECT: Explicit cast
5 cast<f64> 3.0 + // 8
```

---

## Type narrowing with as

The `as` keyword narrows a `ptr` value to a specific struct type. This is a compile-time operation with no runtime cost — the pointer value on the stack is unchanged.

**Syntax:** `value as StructType`

```qd
fn get_name(p:ptr -- name:str) {
	-> p
	p as Dog <<name
}
```

### When to use as

Use `as` when the compiler cannot determine the struct type — typically when a value is typed as `ptr` and multiple structs share a field name:

```qd
struct Foo { value:i64 }
struct Bar { value:str }

fn read_foo(p:ptr -- v:i64) {
	-> p
	p as Foo <<value   // disambiguates <<value
}
```

Without `as`, accessing `<<value` on an untyped `ptr` when multiple structs define that field is a compile error.

### as vs cast

| Feature | `cast<T>` | `as Type` |
|---------|-----------|-----------|
| Purpose | Convert between primitive types | Narrow ptr to struct type |
| Runtime cost | Yes (conversion code) | None (compile-time only) |
| Changes value | Yes | No |
| Target types | `i64`, `f64`, `str`, `ptr` | Any struct type |

### Storing typed locals

Combining `as` with `->` tracks the type for all subsequent accesses:

```qd
fn process(p:ptr -- ) {
	-> p
	p as Point -> pt   // pt is now typed as Point
	pt <<x print nl     // no ambiguity
	pt <<y print nl     // type still known
}
```
