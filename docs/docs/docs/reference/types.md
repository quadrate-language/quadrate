# Type Casting

Operations for converting between types.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `cast<T>` | `( val -- T )` | Convert to specified type |

---

## Conversions

### cast

Converts a value to the specified type using the `cast<T>` syntax.

**Signature:** `( val -- T )`

**Examples:**

```qd
3.14 cast<i64>   // 3 (truncates toward zero)
42 cast<f64>     // 42.0
42 cast<str>     // "42"
"3.14" cast<f64> // 3.14 (parses string)
"42" cast<i64>   // 42 (parses string)
```

**Supported types:** `i64`, `f64`, `str`, `ptr`

---

## Type System

### Basic Types

| Type | Description | Size |
|------|-------------|------|
| `i64` | 64-bit signed integer | 8 bytes |
| `f64` | 64-bit floating-point | 8 bytes |
| `str` | String | Variable |
| `ptr` | Pointer | 8 bytes |
| `bool` | Boolean (alias for i64) | 8 bytes |

### Type Declarations

In function signatures:

```qd
fn process(x:i64 y:f64 name:str data:ptr -- result:i64) {
	// ...
}
```

### Type Checking

Types are checked at compile time. The compiler verifies stack effects match declared signatures.

### Implicit Conversions

Quadrate does not perform implicit type conversions. Use explicit casts:

```qd
// WRONG: Type mismatch
5 3.0 + // Error!

// CORRECT: Explicit cast
5 cast<f64> 3.0 + // 8.0
```
