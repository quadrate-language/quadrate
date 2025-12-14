# `use` testing

Testing utilities for unit tests.

Provides assertion functions for writing test cases.
Tests are defined using the `test` keyword and assertions
check that conditions are met.

@example
use testing

test "addition works" {
    2 3 + 5 testing::assert_eq
}

## Functions

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

