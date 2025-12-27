# `use` json

JSON parsing and querying without AST construction.

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `Array` | `4` | Type: Array value. |
| `Bool` | `1` | Type: Boolean value. |
| `Null` | `0` | Type: Null value. |
| `Number` | `2` | Type: Number value. |
| `Object` | `5` | Type: Object value. |
| `String` | `3` | Type: String value. |

## Functions

### `fn` array_get_float

Get array element as float by index.

**Signature:** `(json:str index:i64 -- value:f64 found:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON array string |
| `index` | `i64` | Element index (0-based) |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `f64` | Float value |
| `found` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"[1.5,2.5]" 0 json::array_get_float  // 1.5 1
```
---

### `fn` array_get_int

Get array element as integer by index.

**Signature:** `(json:str index:i64 -- value:i64 found:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON array string |
| `index` | `i64` | Element index (0-based) |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Integer value |
| `found` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"[10,20,30]" 2 json::array_get_int  // 30 1
```
---

### `fn` array_get_string

Get array element as string by index.

**Signature:** `(json:str index:i64 -- value:str found:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON array string |
| `index` | `i64` | Element index (0-based) |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | String value |
| `found` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"[\"a\",\"b\"]" 1 json::array_get_string  // "b" 1
```
---

### `fn` array_len

Get length of JSON array.

**Signature:** `(json:str -- length:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON array string |

| Output | Type | Description |
|--------|------|-------------|
| `length` | `i64` | Number of elements |

**Example:**

```qd
"[1,2,3]" json::array_len print  // 3
```
---

### `fn` extract_str

Extract string value at position.

**Signature:** `(json:str pos:i64 -- value:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON string |
| `pos` | `i64` | Position of opening quote |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Extracted string |
---

### `fn` find_str_end

Find end of JSON string (pos at opening quote).

**Signature:** `(json:str pos:i64 -- end_pos:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON string |
| `pos` | `i64` | Position of opening quote |

| Output | Type | Description |
|--------|------|-------------|
| `end_pos` | `i64` | Position after closing quote |
---

### `fn` get_array

Get nested array as string by key.

**Signature:** `(json:str key:str -- value:str found:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON object string |
| `key` | `str` | Key to look up |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Array as string |
| `found` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"{\"arr\":[1,2]}" "arr" json::get_array  // "[1,2]" 1
```
---

### `fn` get_bool

Get boolean value by key from object.

**Signature:** `(json:str key:str -- value:i64 found:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON object string |
| `key` | `str` | Key to look up |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | 1 for true, 0 for false |
| `found` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"{\"ok\":true}" "ok" json::get_bool  // 1 1
```
---

### `fn` get_float

Get float value by key from object.

**Signature:** `(json:str key:str -- value:f64 found:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON object string |
| `key` | `str` | Key to look up |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `f64` | Float value |
| `found` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"{\"pi\":3.14}" "pi" json::get_float  // 3.14 1
```
---

### `fn` get_int

Get integer value by key from object.

**Signature:** `(json:str key:str -- value:i64 found:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON object string |
| `key` | `str` | Key to look up |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Integer value |
| `found` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"{\"age\":42}" "age" json::get_int  // 42 1
```
---

### `fn` get_object

Get nested object as string by key.

**Signature:** `(json:str key:str -- value:str found:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON object string |
| `key` | `str` | Key to look up |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Nested object as string |
| `found` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"{\"obj\":{\"x\":1}}" "obj" json::get_object  // "{\"x\":1}" 1
```
---

### `fn` get_string

Get string value by key from object.

**Signature:** `(json:str key:str -- value:str found:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON object string |
| `key` | `str` | Key to look up |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | String value |
| `found` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"{\"name\":\"Bob\"}" "name" json::get_string  // "Bob" 1
```
---

### `fn` has_key

Check if key exists in object.

**Signature:** `(json:str key:str -- exists:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON object string |
| `key` | `str` | Key to check |

| Output | Type | Description |
|--------|------|-------------|
| `exists` | `i64` | 1 if exists, 0 otherwise |

**Example:**

```qd
"{\"a\":1}" "a" json::has_key print  // 1
```
---

### `fn` is_null_at

Check if value at position is null.

**Signature:** `(json:str pos:i64 -- is_null:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON string |
| `pos` | `i64` | Position to check |

| Output | Type | Description |
|--------|------|-------------|
| `is_null` | `i64` | 1 if null, 0 otherwise |

**Example:**

```qd
"null" 0 json::is_null_at print  // 1
```
---

### `fn` type_at

Get JSON value type at position.

**Signature:** `(json:str pos:i64 -- type:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON string |
| `pos` | `i64` | Position to check |

| Output | Type | Description |
|--------|------|-------------|
| `type` | `i64` | Type constant (Null, Bool, Number, String, Array, Object) |

**Example:**

```qd
"{\"a\":1}" 5 json::type_at print  // 2 (Number)
```
