# `use` str

String manipulation functions.
Error codes: Ok=1 (success), specific errors start at 2

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `ErrAlloc` | `3` | Error: Memory allocation failed. |
| `ErrInvalidArg` | `4` | Error: Invalid argument. |
| `ErrOutOfBounds` | `2` | Error: Index out of bounds. |

## Functions

### `fn` char_at

Get character code at index.

**Signature:** `(str:str index:i64 -- char_code:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |
| `index` | `i64` | Character position |

| Output | Type | Description |
|--------|------|-------------|
| `char_code` | `i64` | ASCII/UTF-8 byte value |

| Error | Description |
|-------|-------------|
| `str::ErrOutOfBounds` | Index out of bounds |

**Example:**

```qd
"hello" 0 str::char_at! print  // 104 ('h')
```
---

### `fn` compare

Compare two strings lexicographically.

**Signature:** `(str1:str str2:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str1` | `str` | First string |
| `str2` | `str` | Second string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | <0 if str1<str2, 0 if equal, >0 if str1>str2 |

**Example:**

```qd
"abc" "abd" str::compare print  // -1
```
---

### `fn` concat

Concatenate two strings.

**Signature:** `(str1:str str2:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str1` | `str` | First string |
| `str2` | `str` | Second string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Combined string |

**Example:**

```qd
"hello" " world" str::concat print  // "hello world"
```
---

### `fn` contains

Check if string contains substring.

**Signature:** `(str:str needle:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | String to search in |
| `needle` | `str` | Substring to find |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"hello" "ell" str::contains print  // 1
```
---

### `fn` count

Count non-overlapping occurrences of substring.

**Signature:** `(haystack:str needle:str -- count:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `haystack` | `str` | String to search in |
| `needle` | `str` | Substring to count |

| Output | Type | Description |
|--------|------|-------------|
| `count` | `i64` | Number of occurrences |

**Example:**

```qd
"ababa" "a" str::count print  // 3
"aaa" "aa" str::count print   // 1 (non-overlapping)
```
---

### `fn` ends_with

Check if string ends with suffix.

**Signature:** `(str:str suffix:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | String to check |
| `suffix` | `str` | Suffix to match |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if matches, 0 otherwise |

**Example:**

```qd
"hello" "lo" str::ends_with print  // 1
```
---

### `fn` from_char

Create string from character code.

**Signature:** `(char_code:i64 -- str:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `char_code` | `i64` | ASCII/UTF-8 byte value |

| Output | Type | Description |
|--------|------|-------------|
| `str` | `str` | Single character string |

**Example:**

```qd
65 str::from_char print  // "A"
```
---

### `fn` from_ptr

Convert C string pointer to Quadrate string.

**Signature:** `(ptr:ptr -- str:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `ptr` | `ptr` | Pointer to null-terminated C string |

| Output | Type | Description |
|--------|------|-------------|
| `str` | `str` | Quadrate string |

**Example:**

```qd
entries 0 mem::get_ptr str::from_ptr print
```
---

### `fn` index_of_from

Find substring starting from position.

**Signature:** `(haystack:str needle:str start:i64 -- index:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `haystack` | `str` | String to search in |
| `needle` | `str` | Substring to find |
| `start` | `i64` | Starting position |

| Output | Type | Description |
|--------|------|-------------|
| `index` | `i64` | Index of match, -1 if not found |

**Example:**

```qd
"hello hello" "hello" 1 str::index_of_from print  // 6
```
---

### `fn` index_of

Find first occurrence of substring.

**Signature:** `(haystack:str needle:str -- index:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `haystack` | `str` | String to search in |
| `needle` | `str` | Substring to find |

| Output | Type | Description |
|--------|------|-------------|
| `index` | `i64` | Index of first match, -1 if not found |

**Example:**

```qd
"hello" "ll" str::index_of print  // 2
```
---

### `fn` join

Join array of strings with delimiter.

**Signature:** `(parts:ptr count:i64 delim:str -- result:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `parts` | `ptr` | Array of string pointers |
| `count` | `i64` | Number of strings |
| `delim` | `str` | Delimiter string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Joined string |

| Error | Description |
|-------|-------------|
| `str::ErrAlloc` | Memory allocation failed |

**Example:**

```qd
parts count "/" str::join!  // path
```
---

### `fn` len

Get string length in bytes.

**Signature:** `(str:str -- len:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `len` | `i64` | Length in bytes |

**Example:**

```qd
"hello" str::len print  // 5
```
---

### `fn` last_index_of

Find last occurrence of substring.

**Signature:** `(haystack:str needle:str -- index:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `haystack` | `str` | String to search in |
| `needle` | `str` | Substring to find |

| Output | Type | Description |
|--------|------|-------------|
| `index` | `i64` | Index of last match, -1 if not found |

**Example:**

```qd
"hello hello" "hello" str::last_index_of print  // 6
```
---

### `fn` lower

Convert string to lowercase.

**Signature:** `(str:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Lowercase string |

**Example:**

```qd
"HELLO" str::lower print  // "hello"
```
---

### `fn` repeat

Repeat string n times.

**Signature:** `(str:str n:i64 -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | String to repeat |
| `n` | `i64` | Number of repetitions |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Repeated string |

**Example:**

```qd
"ab" 3 str::repeat print  // "ababab"
```
---

### `fn` reverse

Reverse a string.

**Signature:** `(str:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Reversed string |

**Example:**

```qd
"hello" str::reverse print  // "olleh"
```
---

### `fn` replace

Replace all occurrences of substring.

**Signature:** `(str:str old:str new:str -- result:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Source string |
| `old` | `str` | Substring to replace |
| `new` | `str` | Replacement string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | String with replacements |

| Error | Description |
|-------|-------------|
| `str::ErrAlloc` | Memory allocation failed |

**Example:**

```qd
"hello" "l" "L" str::replace! print  // "heLLo"
```
---

### `fn` sort

Sort array of strings in ascending alphabetical order.

**Signature:** `(arr:ptr count:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of string pointers |
| `count` | `i64` | Number of elements |

**Example:**

```qd
entries count str::sort
```
---

### `fn` sort_desc

Sort array of strings in descending alphabetical order.

**Signature:** `(arr:ptr count:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of string pointers |
| `count` | `i64` | Number of elements |

**Example:**

```qd
entries count str::sort_desc
```
---

### `fn` split

Split string by delimiter.

**Signature:** `(str:str delim:str -- parts:ptr count:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | String to split |
| `delim` | `str` | Delimiter string |

| Output | Type | Description |
|--------|------|-------------|
| `parts` | `ptr` | Array of string parts |
| `count` | `i64` | Number of parts |

| Error | Description |
|-------|-------------|
| `str::ErrAlloc` | Memory allocation failed |

**Example:**

```qd
"a,b,c" "," str::split!  // parts=["a","b","c"], count=3
```
---

### `fn` starts_with

Check if string starts with prefix.

**Signature:** `(str:str prefix:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | String to check |
| `prefix` | `str` | Prefix to match |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if matches, 0 otherwise |

**Example:**

```qd
"hello" "hel" str::starts_with print  // 1
```
---

### `fn` substring

Extract substring.

**Signature:** `(str:str start:i64 length:i64 -- result:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Source string |
| `start` | `i64` | Starting index |
| `length` | `i64` | Number of characters |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Extracted substring |

| Error | Description |
|-------|-------------|
| `str::ErrOutOfBounds` | Index out of bounds |

**Example:**

```qd
"hello" 1 3 str::substring! print  // "ell"
```
---

### `fn` trim

Remove leading and trailing whitespace.

**Signature:** `(str:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Trimmed string |

**Example:**

```qd
"  hello  " str::trim print  // "hello"
```
---

### `fn` trim_left

Remove leading whitespace only.

**Signature:** `(str:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | String with leading whitespace removed |

**Example:**

```qd
"  hello  " str::trim_left print  // "hello  "
```
---

### `fn` trim_right

Remove trailing whitespace only.

**Signature:** `(str:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | String with trailing whitespace removed |

**Example:**

```qd
"  hello  " str::trim_right print  // "  hello"
```
---

### `fn` upper

Convert string to uppercase.

**Signature:** `(str:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Uppercase string |

**Example:**

```qd
"hello" str::upper print  // "HELLO"
```
