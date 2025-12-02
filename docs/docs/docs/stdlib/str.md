# str

String manipulation functions.

## Functions

### char_at

Get character code at index.

**Signature:** `( str:str index:i64 -- char_code:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |
| `index` | `i64` | Character position |

| Return | Type | Description |
|--------|------|-------------|
| `char_code` | `i64` | ASCII/UTF-8 byte value |

**Example:**

```qd
"hello" 0 str::char_at .  // 104 ('h')
```

---

### compare

Compare two strings lexicographically.

**Signature:** `( str1:str str2:str -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str1` | `str` | First string |
| `str2` | `str` | Second string |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | <0 if str1<str2, 0 if equal, >0 if str1>str2 |

**Example:**

```qd
"abc" "abd" str::compare .  // -1
```

---

### concat

Concatenate two strings.

**Signature:** `( str1:str str2:str -- result:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str1` | `str` | First string |
| `str2` | `str` | Second string |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `str` | Combined string |

**Example:**

```qd
"hello" " world" str::concat .  // "hello world"
```

---

### contains

Check if string contains substring.

**Signature:** `( str:str needle:str -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | String to search in |
| `needle` | `str` | Substring to find |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
"hello" "ell" str::contains .  // 1
```

---

### ends_with

Check if string ends with suffix.

**Signature:** `( str:str suffix:str -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | String to check |
| `suffix` | `str` | Suffix to match |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if matches, 0 otherwise |

**Example:**

```qd
"hello" "lo" str::ends_with .  // 1
```

---

### from_char

Create string from character code.

**Signature:** `( char_code:i64 -- str:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `char_code` | `i64` | ASCII/UTF-8 byte value |

| Return | Type | Description |
|--------|------|-------------|
| `str` | `str` | Single character string |

**Example:**

```qd
65 str::from_char .  // "A"
```

---

### index_of

Find first occurrence of substring.

**Signature:** `( haystack:str needle:str -- index:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `haystack` | `str` | String to search in |
| `needle` | `str` | Substring to find |

| Return | Type | Description |
|--------|------|-------------|
| `index` | `i64` | Index of first match, -1 if not found |

**Example:**

```qd
"hello" "ll" str::index_of .  // 2
```

---

### index_of_from

Find substring starting from position.

**Signature:** `( haystack:str needle:str start:i64 -- index:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `haystack` | `str` | String to search in |
| `needle` | `str` | Substring to find |
| `start` | `i64` | Starting position |

| Return | Type | Description |
|--------|------|-------------|
| `index` | `i64` | Index of match, -1 if not found |

**Example:**

```qd
"hello hello" "hello" 1 str::index_of_from .  // 6
```

---

### len

Get string length in bytes.

**Signature:** `( str:str -- len:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |

| Return | Type | Description |
|--------|------|-------------|
| `len` | `i64` | Length in bytes |

**Example:**

```qd
"hello" str::len .  // 5
```

---

### lower

Convert string to lowercase.

**Signature:** `( str:str -- result:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `str` | Lowercase string |

**Example:**

```qd
"HELLO" str::lower .  // "hello"
```

---

### replace

Replace all occurrences of substring.

**Signature:** `( str:str old:str new:str -- result:str )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Source string |
| `old` | `str` | Substring to replace |
| `new` | `str` | Replacement string |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `str` | String with replacements |

**Errors:**

- Memory allocation failed

**Example:**

```qd
"hello" "l" "L" str::replace! .  // "heLLo"
```

---

### split

Split string by delimiter.

**Signature:** `( str:str delim:str -- parts:ptr count:i64 )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | String to split |
| `delim` | `str` | Delimiter string |

| Return | Type | Description |
|--------|------|-------------|
| `parts` | `ptr` | Array of string parts |
| `count` | `i64` | Number of parts |

**Errors:**

- Memory allocation failed

**Example:**

```qd
"a,b,c" "," str::split!  // parts=["a","b","c"], count=3
```

---

### starts_with

Check if string starts with prefix.

**Signature:** `( str:str prefix:str -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | String to check |
| `prefix` | `str` | Prefix to match |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if matches, 0 otherwise |

**Example:**

```qd
"hello" "hel" str::starts_with .  // 1
```

---

### substring

Extract substring.

**Signature:** `( str:str start:i64 length:i64 -- result:str )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Source string |
| `start` | `i64` | Starting index |
| `length` | `i64` | Number of characters |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `str` | Extracted substring |

**Errors:**

- Out of bounds

**Example:**

```qd
"hello" 1 3 str::substring! .  // "ell"
```

---

### trim

Remove leading and trailing whitespace.

**Signature:** `( str:str -- result:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `str` | Trimmed string |

**Example:**

```qd
"  hello  " str::trim .  // "hello"
```

---

### upper

Convert string to uppercase.

**Signature:** `( str:str -- result:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | `str` | Input string |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `str` | Uppercase string |

**Example:**

```qd
"hello" str::upper .  // "HELLO"
```
