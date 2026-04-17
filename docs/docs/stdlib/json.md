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

### `fn` array_get_array

Get array element as nested array by index.

**Signature:** `(json:str index:i64 -- value:str found:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON array string |
| `index` | `i64` | Element index (0-based) |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Nested array as JSON string |
| `found` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"[[1,2],[3,4]]" 0 json::array_get_array  // "[1,2]" 1
```
---

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

### `fn` array_get_object

Get array element as object by index.

**Signature:** `(json:str index:i64 -- value:str found:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON array string |
| `index` | `i64` | Element index (0-based) |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Object as JSON string |
| `found` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"[{\"a\":1},{\"b\":2}]" 1 json::array_get_object  // "{\"b\":2}" 1
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

### `fn` arr

Build a JSON array from comma-separated elements.

**Signature:** `(content:str -- arr:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `content` | `str` | Comma-separated elements |

| Output | Type | Description |
|--------|------|-------------|
| `arr` | `str` | JSON array |

**Example:**

```qd
"1,2,3" json::arr print  // [1,2,3]
```
---

### `fn` empty_arr

Build an empty JSON array.

**Signature:** `( -- arr:str)`

| Output | Type | Description |
|--------|------|-------------|
| `arr` | `str` | Empty JSON array |

**Example:**

```qd
json::empty_arr print  // []
```
---

### `fn` empty_obj

Build an empty JSON object.

**Signature:** `( -- obj:str)`

| Output | Type | Description |
|--------|------|-------------|
| `obj` | `str` | Empty JSON object |

**Example:**

```qd
json::empty_obj print  // {}
```
---

### `fn` encode

Encode a string for JSON (add quotes and escape special chars).

**Signature:** `(s:str -- encoded:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to encode |

| Output | Type | Description |
|--------|------|-------------|
| `encoded` | `str` | JSON-encoded string with quotes |

**Example:**

```qd
"hello" json::encode print  // "hello"
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

### `fn` join

Join multiple JSON fragments with commas.

**Signature:** `(a:str b:str -- joined:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `str` | First fragment |
| `b` | `str` | Second fragment |

| Output | Type | Description |
|--------|------|-------------|
| `joined` | `str` | Comma-joined fragments |

**Example:**

```qd
"\"a\":1" "\"b\":2" json::join print  // "a":1,"b":2
```
---

### `fn` kv_bool

Build a key-value pair with boolean value.

**Signature:** `(key:str value:i64 -- kv:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `key` | `str` | Key name |
| `value` | `i64` | Boolean value (0=false, non-zero=true) |

| Output | Type | Description |
|--------|------|-------------|
| `kv` | `str` | JSON key-value pair |

**Example:**

```qd
"active" 1 json::kv_bool print  // "active":true
```
---

### `fn` kv_int

Build a key-value pair with integer value.

**Signature:** `(key:str value:i64 -- kv:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `key` | `str` | Key name |
| `value` | `i64` | Integer value |

| Output | Type | Description |
|--------|------|-------------|
| `kv` | `str` | JSON key-value pair |

**Example:**

```qd
"age" 42 json::kv_int print  // "age":42
```
---

### `fn` kv

Build a key-value pair with string value.

**Signature:** `(key:str value:str -- kv:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `key` | `str` | Key name |
| `value` | `str` | String value |

| Output | Type | Description |
|--------|------|-------------|
| `kv` | `str` | JSON key-value pair |

**Example:**

```qd
"name" "Bob" json::kv print  // "name":"Bob"
```
---

### `fn` kv_raw

Build a key-value pair with raw JSON value (object, array, or literal).

**Signature:** `(key:str value:str -- kv:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `key` | `str` | Key name |
| `value` | `str` | Raw JSON value (not quoted) |

| Output | Type | Description |
|--------|------|-------------|
| `kv` | `str` | JSON key-value pair |

**Example:**

```qd
"data" "{}" json::kv_raw print  // "data":{}
```
---

### `fn` obj

Build a JSON object from comma-separated key-value pairs.

**Signature:** `(content:str -- obj:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `content` | `str` | Comma-separated key-value pairs |

| Output | Type | Description |
|--------|------|-------------|
| `obj` | `str` | JSON object |

**Example:**

```qd
"\"a\":1,\"b\":2" json::obj print  // {"a":1,"b":2}
```
---

### `fn` type_at

Get JSON value type at position.

**Signature:** `(json:str pos:i64 -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `json` | `str` | JSON string |
| `pos` | `i64` | Position to check |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | Type constant (Null, Bool, Number, String, Array, Object) |

**Example:**

```qd
"{\"a\":1}" 5 json::type_at print  // 2 (Number)
```
