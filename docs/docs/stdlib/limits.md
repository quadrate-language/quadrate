# `use` limits

Numeric limits and constants.
Provides minimum and maximum values for numeric types.

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `F64Epsilon` | `2.220446049250313e-16` | Machine epsilon for f64 (smallest x such that 1.0 + x != 1.0). |
| `F64Max` | `1.7976931348623157e+308` | Largest finite f64 value. |
| `F64Min` | `2.2250738585072014e-308` | Smallest positive normalized f64 value. |
| `F64Size` | `8` | Size of f64 in bytes. |
| `I16Max` | `32767` | Maximum value for 16-bit signed integer (C: int16_t). |
| `I16Min` | `-32768` | Minimum value for 16-bit signed integer (C: int16_t). |
| `I32Max` | `2147483647` | Maximum value for 32-bit signed integer (C: int32_t). |
| `I32Min` | `-2147483648` | Minimum value for 32-bit signed integer (C: int32_t). |
| `I64Max` | `9223372036854775807` | Maximum value for i64 (Quadrate's native integer type). |
| `I64Size` | `8` | Size of i64 in bytes. |
| `I8Max` | `127` | Maximum value for 8-bit signed integer (C: int8_t). |
| `I8Min` | `-128` | Minimum value for 8-bit signed integer (C: int8_t). |
| `PtrSize` | `8` | Size of a pointer in bytes (64-bit platform). |
| `U16Max` | `65535` | Maximum value for 16-bit unsigned integer (C: uint16_t). |
| `U32Max` | `4294967295` | Maximum value for 32-bit unsigned integer (C: uint32_t). |
| `U8Max` | `255` | Maximum value for 8-bit unsigned integer (C: uint8_t). |

## Functions

### `fn` clamp_i16

Clamp value to i16 range.

**Signature:** `(v:i64 -- clamped:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `i64` | Value to clamp |

| Output | Type | Description |
|--------|------|-------------|
| `clamped` | `i64` | Value clamped to [-32768, 32767] |

**Example:**

```qd
50000 limits::clamp_i16 print  // 32767
```
---

### `fn` clamp_i8

Clamp value to i8 range.

**Signature:** `(v:i64 -- clamped:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `i64` | Value to clamp |

| Output | Type | Description |
|--------|------|-------------|
| `clamped` | `i64` | Value clamped to [-128, 127] |

**Example:**

```qd
200 limits::clamp_i8 print  // 127
```
---

### `fn` clamp_u16

Clamp value to u16 range.

**Signature:** `(v:i64 -- clamped:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `i64` | Value to clamp |

| Output | Type | Description |
|--------|------|-------------|
| `clamped` | `i64` | Value clamped to [0, 65535] |

**Example:**

```qd
70000 limits::clamp_u16 print  // 65535
```
---

### `fn` clamp_u8

Clamp value to u8 range.

**Signature:** `(v:i64 -- clamped:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `i64` | Value to clamp |

| Output | Type | Description |
|--------|------|-------------|
| `clamped` | `i64` | Value clamped to [0, 255] |

**Example:**

```qd
300 limits::clamp_u8 print  // 255
```
---

### `fn` fits_i16

Check if value fits in i16 range.

**Signature:** `(v:i64 -- fits:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `i64` | Value to check |

| Output | Type | Description |
|--------|------|-------------|
| `fits` | `i64` | 1 if in range, 0 otherwise |

**Example:**

```qd
30000 limits::fits_i16 print  // 1
```
---

### `fn` fits_i32

Check if value fits in i32 range.

**Signature:** `(v:i64 -- fits:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `i64` | Value to check |

| Output | Type | Description |
|--------|------|-------------|
| `fits` | `i64` | 1 if in range, 0 otherwise |

**Example:**

```qd
2000000000 limits::fits_i32 print  // 1
```
---

### `fn` fits_i8

Check if value fits in i8 range.

**Signature:** `(v:i64 -- fits:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `i64` | Value to check |

| Output | Type | Description |
|--------|------|-------------|
| `fits` | `i64` | 1 if in range, 0 otherwise |

**Example:**

```qd
100 limits::fits_i8 print  // 1
```
---

### `fn` fits_u16

Check if value fits in u16 range.

**Signature:** `(v:i64 -- fits:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `i64` | Value to check |

| Output | Type | Description |
|--------|------|-------------|
| `fits` | `i64` | 1 if in range, 0 otherwise |

**Example:**

```qd
50000 limits::fits_u16 print  // 1
```
---

### `fn` fits_u32

Check if value fits in u32 range.

**Signature:** `(v:i64 -- fits:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `i64` | Value to check |

| Output | Type | Description |
|--------|------|-------------|
| `fits` | `i64` | 1 if in range, 0 otherwise |

**Example:**

```qd
3000000000 limits::fits_u32 print  // 1
```
---

### `fn` fits_u8

Check if value fits in u8 range.

**Signature:** `(v:i64 -- fits:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `i64` | Value to check |

| Output | Type | Description |
|--------|------|-------------|
| `fits` | `i64` | 1 if in range, 0 otherwise |

**Example:**

```qd
200 limits::fits_u8 print  // 1
```
---

### `fn` I64Min

Minimum value for i64 (Quadrate's native integer type). Note: -9223372036854775808 cannot be a literal (parser overflows), so we compute it.

**Signature:** `( -- result:i64)`
