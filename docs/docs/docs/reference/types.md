# Type Casting

Operations for converting between types.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `casti` | `( val -- i64 )` | Convert to integer |
| `castf` | `( val -- f64 )` | Convert to float |
| `casts` | `( val -- str )` | Convert to string |

---

## Conversions

### casti

Converts a value to an integer.

**Signature:** `( val -- i64 )`

```qd
3.14 casti // 3
```

Truncates floating-point values toward zero.

### castf

Converts a value to a float.

**Signature:** `( val -- f64 )`

```qd
42 castf // 42.0
```

### casts

Converts a value to a string.

**Signature:** `( val -- str )`

```qd
42 casts // "42"
```

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
5 castf 3.0 + // 8.0
```
