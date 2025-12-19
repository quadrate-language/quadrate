# `use` strconv

String to number conversions.

Functions:
- format_int: Format integer in any base (2-36)
- parse_int: Parse string to integer in any base (2-36)
- itoa: Convert integer to decimal string (shorthand for 10 format_int)
- atoi: Parse decimal string to integer (shorthand for 10 parse_int)

## Functions

### `fn` atoi

Parse decimal string to integer (base 10). Equivalent to: str 10 strconv::parse_int

**Signature:** `(s:str -- value:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Decimal string |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Parsed integer |

**Example:**

```qd
"42" strconv::atoi print  // 42
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

### `fn` parse_int

Parse integer from string in given base.

**Signature:** `(s:str base:i64 -- value:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to parse |
| `base` | `i64` | Numeric base (2-36) |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Parsed integer |

**Example:**

```qd
"ff" 16 strconv::parse_int print  // 255
```

