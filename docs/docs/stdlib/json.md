# `use` json

JSON parsing and querying.

Two ways in. `parse` reads a document once into a Value tree, which is what
you want when more than one field is read out of it. The get_* family scans
the text for a single key and allocates nothing, which is cheaper for exactly
that -- one lookup, one document.

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `Array` | `4` | Type: Array value. |
| `Bool` | `1` | Type: Boolean value. |
| `ErrDepth` | `14` | Error: the document nests deeper than MaxDepth. |
| `ErrIndex` | `12` | Error: the array index is out of range. |
| `ErrKey` | `13` | Error: the object has no member with that key. |
| `ErrSyntax` | `10` | Error: the document is not well-formed JSON. |
| `ErrType` | `11` | Error: the value is not of the requested type. |
| `MaxDepth` | `200` | Deepest array/object nesting `parse` accepts. Parsing is recursive, so this is what stops a document like [[[[... from running the process out of stack. |
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
## Value

A parsed JSON value.  Children are a singly linked list rather than an array: `head` is the first element (or member), `next` chains siblings, and `tail` is kept only so that appending stays O(1). An object member carries its name in `key`. 

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `kind` | `i64` | One of Null, Bool, Number, String, Array, Object |
| `inum` | `i64` | Bool payload (0 or 1), or the Number payload when isint |
| `num` | `f64` | Number payload, always set for a Number |
| `isint` | `i64` | 1 when the number was written without fraction or exponent |
| `text` | `str` | String payload |
| `key` | `str` | Member name, when this value is a member of an object |
| `head` | `ptr` | First child, or null |
| `tail` | `ptr` | Last child, or null |
| `next` | `ptr` | Next sibling, or null |
| `count` | `i64` | Number of children |

### Constructors

#### `fn` new_array

Create an empty array.

**Signature:** `( -- v:Value)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Value` | JSON array |

**Example:**

```qd
json::new_array  // v
```
---

#### `fn` new_bool

Create a boolean value.

**Signature:** `(b:i64 -- v:Value)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `b` | `i64` | 0 for false, anything else for true |

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Value` | JSON boolean |

**Example:**

```qd
1 json::new_bool  // v
```
---

#### `fn` new_float

Create a number from a float.

**Signature:** `(f:f64 -- v:Value)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `f64` | Floating point value |

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Value` | JSON number |

**Example:**

```qd
1.5 json::new_float  // v
```
---

#### `fn` new_int

Create a number from an integer.

**Signature:** `(n:i64 -- v:Value)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `n` | `i64` | Integer value |

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Value` | JSON number |

**Example:**

```qd
42 json::new_int  // v
```
---

#### `fn` new_null

Create a null value.

**Signature:** `( -- v:Value)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Value` | JSON null |

**Example:**

```qd
json::new_null  // v
```
---

#### `fn` new_object

Create an empty object.

**Signature:** `( -- v:Value)`

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Value` | JSON object |

**Example:**

```qd
json::new_object  // v
```
---

#### `fn` new_string

Create a string value.

**Signature:** `(s:str -- v:Value)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String contents (unescaped) |

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Value` | JSON string |

**Example:**

```qd
"hello" json::new_string  // v
```
---

#### `fn` parse

Parse a JSON document into a Value tree.  The whole document is read once, so a lookup on the result costs a walk of the tree rather than a rescan of the text -- which is what the get_* family above does on every call. Failure reports the byte offset. 

**Signature:** `(text:str -- v:Value)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `text` | `str` | JSON document |

| Output | Type | Description |
|--------|------|-------------|
| `v` | `Value` | Parsed value |

**Example:**

```qd
"{\x22a\x22:1}" json::parse!  // doc
```

### Methods

#### `fn` as_bool

Read a boolean value.

**Signature:** `(v:Value) as_bool( -- b:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Boolean value |

| Output | Type | Description |
|--------|------|-------------|
| `b` | `i64` | 0 or 1 |

**Example:**

```qd
v json::as_bool!  // b
```
---

#### `fn` as_float

Read a number as a float.

**Signature:** `(v:Value) as_float( -- f:f64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Number value |

| Output | Type | Description |
|--------|------|-------------|
| `f` | `f64` | Floating point value |

**Example:**

```qd
v json::as_float!  // f
```
---

#### `fn` as_int

Read a number as an integer. A number written with a fraction or an exponent is truncated toward zero.

**Signature:** `(v:Value) as_int( -- n:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Number value |

| Output | Type | Description |
|--------|------|-------------|
| `n` | `i64` | Integer value |

**Example:**

```qd
v json::as_int!  // n
```
---

#### `fn` as_str

Read a string value.

**Signature:** `(v:Value) as_str( -- s:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | String value |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | String contents, with escapes already decoded |

**Example:**

```qd
v json::as_str!  // s
```
---

#### `fn` at

Read the element at an index. Works on an object too, where it reads the member at that position.

**Signature:** `(v:Value) at(index:i64 -- child:Value)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Array or object |
| `index` | `i64` | Zero-based position |

| Output | Type | Description |
|--------|------|-------------|
| `child` | `Value` | Value at that position |

**Example:**

```qd
doc 0 json::at!  // first
```
---

#### `fn` first

Read the first child of an array or object.

**Signature:** `(v:Value) first( -- child:Value)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Array or object with at least one child |

| Output | Type | Description |
|--------|------|-------------|
| `child` | `Value` | First child |

**Example:**

```qd
doc json::first!  // c
```
---

#### `fn` get

Read an object member by name.

**Signature:** `(v:Value) get(key:str -- child:Value)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Object |
| `key` | `str` | Member name |

| Output | Type | Description |
|--------|------|-------------|
| `child` | `Value` | Member value |

**Example:**

```qd
doc "name" json::get!  // name_value
```
---

#### `fn` has_next

Report whether this value has a sibling after it.

**Signature:** `(v:Value) has_next( -- b:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Child value |

| Output | Type | Description |
|--------|------|-------------|
| `b` | `i64` | 1 when a sibling follows |

**Example:**

```qd
c json::has_next  // b
```
---

#### `fn` has

Report whether an object has a member with this name.

**Signature:** `(v:Value) has(key:str -- exists:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Object |
| `key` | `str` | Member name |

| Output | Type | Description |
|--------|------|-------------|
| `exists` | `i64` | 1 when the member is present |

**Example:**

```qd
doc "name" json::has  // exists
```
---

#### `fn` is_array

Report whether the value is an array.

**Signature:** `(v:Value) is_array( -- b:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Value to test |

| Output | Type | Description |
|--------|------|-------------|
| `b` | `i64` | 1 when the value is an array |

**Example:**

```qd
v json::is_array  // b
```
---

#### `fn` is_bool

Report whether the value is a boolean.

**Signature:** `(v:Value) is_bool( -- b:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Value to test |

| Output | Type | Description |
|--------|------|-------------|
| `b` | `i64` | 1 when the value is a boolean |

**Example:**

```qd
v json::is_bool  // b
```
---

#### `fn` is_null

Report whether the value is JSON null.

**Signature:** `(v:Value) is_null( -- b:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Value to test |

| Output | Type | Description |
|--------|------|-------------|
| `b` | `i64` | 1 when the value is null |

**Example:**

```qd
v json::is_null  // b
```
---

#### `fn` is_number

Report whether the value is a number.

**Signature:** `(v:Value) is_number( -- b:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Value to test |

| Output | Type | Description |
|--------|------|-------------|
| `b` | `i64` | 1 when the value is a number |

**Example:**

```qd
v json::is_number  // b
```
---

#### `fn` is_object

Report whether the value is an object.

**Signature:** `(v:Value) is_object( -- b:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Value to test |

| Output | Type | Description |
|--------|------|-------------|
| `b` | `i64` | 1 when the value is an object |

**Example:**

```qd
v json::is_object  // b
```
---

#### `fn` is_string

Report whether the value is a string.

**Signature:** `(v:Value) is_string( -- b:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Value to test |

| Output | Type | Description |
|--------|------|-------------|
| `b` | `i64` | 1 when the value is a string |

**Example:**

```qd
v json::is_string  // b
```
---

#### `fn` len

Count the elements of an array or the members of an object. Any other value has no children and counts 0.

**Signature:** `(v:Value) len( -- n:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Value to measure |

| Output | Type | Description |
|--------|------|-------------|
| `n` | `i64` | Number of children |

**Example:**

```qd
v json::len  // n
```
---

#### `fn` name

Read the name this value carries as a member of an object. Empty for a value that is not a member.

**Signature:** `(v:Value) name( -- k:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Member value |

| Output | Type | Description |
|--------|------|-------------|
| `k` | `str` | Member name |

**Example:**

```qd
v json::name  // k
```
---

#### `fn` next

Read the sibling after this value. Walking `first` then `next` visits the children of an array or object in order, in linear time overall -- `at` walks from the head on every call.

**Signature:** `(v:Value) next( -- sibling:Value)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Child value |

| Output | Type | Description |
|--------|------|-------------|
| `sibling` | `Value` | Next sibling |

**Example:**

```qd
c json::next!  // c
```
---

#### `fn` pretty

Serialize a value to indented JSON text.

**Signature:** `(v:Value) pretty(width:i64 -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Value to serialize |
| `width` | `i64` | Spaces per nesting level |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | JSON text, one element or member to a line |

**Example:**

```qd
doc 2 json::pretty print
```
---

#### `fn` push

Append a value to an array.

**Signature:** `(v:Value) push(child:Value -- v2:Value)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Array |
| `child` | `Value` | Value to append |

| Output | Type | Description |
|--------|------|-------------|
| `v2` | `Value` | The same array |

**Example:**

```qd
arr 42 json::new_int json::push!  // arr
```
---

#### `fn` put

Set an object member. Replaces the member of that name if there is one, keeping its position.

**Signature:** `(v:Value) put(key:str child:Value -- v2:Value)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Object |
| `key` | `str` | Member name |
| `child` | `Value` | Member value |

| Output | Type | Description |
|--------|------|-------------|
| `v2` | `Value` | The same object |

**Example:**

```qd
obj "n" 1 json::new_int json::put!  // obj
```
---

#### `fn` stringify

Serialize a value to compact JSON text.

**Signature:** `(v:Value) stringify( -- s:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `v` | `Value` | Value to serialize |

| Output | Type | Description |
|--------|------|-------------|
| `s` | `str` | JSON text with no space between tokens |

**Example:**

```qd
doc json::stringify print
```

