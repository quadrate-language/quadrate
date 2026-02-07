# `use` strconv

String to number conversions.

Functions:
- format_int: Format integer in any base (2-36)
- parse_int: Parse string to integer in any base (2-36) *(fallible)*
- itoa: Convert integer to decimal string (shorthand for 10 format_int)
- atoi: Parse decimal string to integer (shorthand for 10 parse_int) *(fallible)*
- format_float: Format float to string
- parse_float: Parse string to float *(fallible)*
- format_bool: Format boolean to string
- parse_bool: Parse string to boolean *(fallible)*

## Functions

### `fn` itoa

Convert integer to decimal string (base 10). Equivalent to: value 10 strconv::format_int

**Signature:** `(value:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Integer value |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | Decimal string |

**Example:**

```qd
42 strconv::itoa print  // "42"
```
---

### `fn` atoi `!`

Parse decimal string to integer (base 10). This is a fallible function — use `!` to abort on error, or `switch` to handle errors.

**Signature:** `(s:str -- value:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Decimal string |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Parsed integer |

**Example:**

```qd
"42" strconv::atoi! print  // 42

// With error handling:
"abc" strconv::atoi switch {
    Ok { -> val val print nl }
    _ { err -> code -> msg "parse failed" print nl }
}
```
---

### `fn` format_int

Format integer in given base.

**Signature:** `(value:i64 base:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Integer value |
| `base` | `i64` | Numeric base (2-36) |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | Formatted string |

**Example:**

```qd
255 16 strconv::format_int print  // "ff"
```
---

### `fn` parse_int `!`

Parse integer from string in given base. This is a fallible function — use `!` to abort on error, or `switch` to handle errors.

**Signature:** `(s:str base:i64 -- value:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to parse |
| `base` | `i64` | Numeric base (2-36) |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Parsed integer |

**Example:**

```qd
"ff" 16 strconv::parse_int! print  // 255
```
---

### `fn` format_float

Format float to string. Uses compact %g format (removes trailing zeros).

**Signature:** `(value:f64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `f64` | Float value |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | Formatted string |

**Example:**

```qd
3.14159 strconv::format_float print  // "3.14159"
```
---

### `fn` parse_float `!`

Parse string to float. This is a fallible function — use `!` to abort on error, or `switch` to handle errors.

**Signature:** `(s:str -- value:f64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to parse |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `f64` | Parsed float |

**Example:**

```qd
"3.14" strconv::parse_float! print  // 3.14
```
---

### `fn` format_bool

Format boolean to string. Non-zero becomes "true", zero becomes "false".

**Signature:** `(value:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Boolean value (0 or non-zero) |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | "true" or "false" |

**Example:**

```qd
1 strconv::format_bool print  // "true"
```
---

### `fn` parse_bool `!`

Parse string to boolean. Accepts "true"/"1" as true, "false"/"0" as false (case-insensitive). This is a fallible function — use `!` to abort on error, or `switch` to handle errors.

**Signature:** `(s:str -- value:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to parse |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | 1 for true, 0 for false |

**Example:**

```qd
"true" strconv::parse_bool! print  // 1
```
