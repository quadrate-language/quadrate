# strconv

String to number conversions.

## Functions

### atoi

Parse decimal string to integer.

**Signature:** `( str:str -- value:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Decimal string |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Parsed integer |

**Example:**

```qd
"42" strconv::atoi .  // 42
```

---

### format_int

Format integer in given base.

**Signature:** `( value:i64 base:i64 -- str:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Integer value |
| `base` | `i64` | Numeric base (2-36) |

| Output | Type | Description |
|--------|------|-------------|
| `str` | `str` | Formatted string |

**Example:**

```qd
255 16 strconv::format_int .  // "ff"
```

---

### itoa

Convert integer to decimal string.

**Signature:** `( value:i64 -- str:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `i64` | Integer value |

| Output | Type | Description |
|--------|------|-------------|
| `str` | `str` | Decimal string |

**Example:**

```qd
42 strconv::itoa .  // "42"
```

---

### parse_int

Parse integer from string in given base.

**Signature:** `( str:str base:i64 -- value:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | String to parse |
| `base` | `i64` | Numeric base (2-36) |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Parsed integer |

**Example:**

```qd
"ff" 16 strconv::parse_int .  // 255
```
