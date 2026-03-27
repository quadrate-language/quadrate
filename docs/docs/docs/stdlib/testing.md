# `use` testing

Testing utilities for unit tests.

Provides assertion functions for writing test cases.
Tests are defined using the `test` keyword and assertions
check that conditions are met.

<<example
use testing

test "addition works" {
    2 3 + 5 testing::assert_eq
}

## Functions

### `fn` assert_approx_eq

Assert that two floats are approximately equal within epsilon. Useful for comparing floating-point numbers which may have rounding errors.

**Signature:** `(a:f64 b:f64 epsilon:f64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `f64` | First value |
| `b` | `f64` | Second value (expected) |
| `epsilon` | `f64` | Maximum allowed difference |

**Example:**

```qd
3.14159 3.14160 0.0001 testing::assert_approx_eq  // passes
```
---

### `fn` assert_contains

Assert that a string contains a substring.

**Signature:** `(haystack:str needle:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `haystack` | `str` | String to search in |
| `needle` | `str` | Substring to search for |

**Example:**

```qd
"hello world" "world" testing::assert_contains  // passes
```
---

### `fn` assert_ends_with

Assert that a string ends with a suffix.

**Signature:** `(s:str suffix:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to check |
| `suffix` | `str` | Expected suffix |

**Example:**

```qd
"hello world" "world" testing::assert_ends_with  // passes
```
---

### `fn` assert_eq

Assert that two values are equal. Works with any type (i64, f64, str, ptr).

**Signature:** `(a:any b:any -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `any` | First value |
| `b` | `any` | Second value (expected) |

**Example:**

```qd
5 5 testing::assert_eq  // passes
"hello" "hello" testing::assert_eq  // passes
```
---

### `fn` assert_false

Assert that a value is falsy. Falsy means: zero for integers, zero for floats, empty for strings, null for pointers.

**Signature:** `(v:any -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `any` | Value to check |

**Example:**

```qd
0 testing::assert_false  // passes
"" testing::assert_false  // passes
```
---

### `fn` assert_ge

Assert that a is greater than or equal to b.

**Signature:** `(a:i64 b:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `i64` | First value |
| `b` | `i64` | Second value |

**Example:**

```qd
5 5 testing::assert_ge  // passes
```
---

### `fn` assert_gt

Assert that a is greater than b.

**Signature:** `(a:i64 b:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `i64` | First value |
| `b` | `i64` | Second value |

**Example:**

```qd
5 3 testing::assert_gt  // passes
```
---

### `fn` assert_gt_f

Assert that a float is greater than another.

**Signature:** `(a:f64 b:f64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `f64` | First value |
| `b` | `f64` | Second value |

**Example:**

```qd
5.0 3.0 testing::assert_gt_f  // passes
```
---

### `fn` assert_in_range

Assert that value is within an inclusive range [min, max].

**Signature:** `(value:i64 min:i64 max:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to check |
| `min` | `i64` | Minimum value (inclusive) |
| `max` | `i64` | Maximum value (inclusive) |

**Example:**

```qd
5 0 10 testing::assert_in_range  // passes
```
---

### `fn` assert_le

Assert that a is less than or equal to b.

**Signature:** `(a:i64 b:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `i64` | First value |
| `b` | `i64` | Second value |

**Example:**

```qd
5 5 testing::assert_le  // passes
```
---

### `fn` assert_lt

Assert that a is less than b.

**Signature:** `(a:i64 b:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `i64` | First value |
| `b` | `i64` | Second value |

**Example:**

```qd
3 5 testing::assert_lt  // passes
```
---

### `fn` assert_lt_f

Assert that a float is less than another.

**Signature:** `(a:f64 b:f64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `f64` | First value |
| `b` | `f64` | Second value |

**Example:**

```qd
3.0 5.0 testing::assert_lt_f  // passes
```
---

### `fn` assert_ne

Assert that two values are not equal. Works with any type (i64, f64, str, ptr).

**Signature:** `(a:any b:any -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `any` | First value |
| `b` | `any` | Second value |

**Example:**

```qd
5 6 testing::assert_ne  // passes
```
---

### `fn` assert_negative

Assert that value is negative (< 0).

**Signature:** `(value:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to check |

**Example:**

```qd
-5 testing::assert_negative  // passes
```
---

### `fn` assert_nonzero

Assert that value is non-zero.

**Signature:** `(value:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to check |

**Example:**

```qd
5 testing::assert_nonzero  // passes
```
---

### `fn` assert_positive

Assert that value is positive (> 0).

**Signature:** `(value:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to check |

**Example:**

```qd
5 testing::assert_positive  // passes
```
---

### `fn` assert_starts_with

Assert that a string starts with a prefix.

**Signature:** `(s:str prefix:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to check |
| `prefix` | `str` | Expected prefix |

**Example:**

```qd
"hello world" "hello" testing::assert_starts_with  // passes
```
---

### `fn` assert_true

Assert that a value is truthy. Truthy means: non-zero for integers, non-zero for floats, non-empty for strings, non-null for pointers.

**Signature:** `(v:any -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `any` | Value to check |

**Example:**

```qd
1 testing::assert_true  // passes
"hello" testing::assert_true  // passes
```
---

### `fn` assert_zero

Assert that value is zero.

**Signature:** `(value:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Value to check |

**Example:**

```qd
0 testing::assert_zero  // passes
```
---

### `fn` fail

Unconditionally fail a test with a message.

**Signature:** `(msg:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `msg` | `str` | Failure message |

**Example:**

```qd
"Not implemented" testing::fail
```
