# `use` strings

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
| `strings::ErrOutOfBounds` | Index out of bounds |

**Example:**

```qd
"hello" 0 strings::char_at! print  // 104 ('h')
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
"abc" "abd" strings::compare print  // -1
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
"hello" " world" strings::concat print  // "hello world"
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
"hello" "ell" strings::contains print  // 1
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
"ababa" "a" strings::count print  // 3
"aaa" "aa" strings::count print   // 1 (non-overlapping)
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
"hello" "lo" strings::ends_with print  // 1
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
65 strings::from_char print  // "A"
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
entries 0 mem::get_ptr strings::from_ptr print
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
"hello hello" "hello" 1 strings::index_of_from print  // 6
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
"hello" "ll" strings::index_of print  // 2
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
| `strings::ErrAlloc` | Memory allocation failed |

**Example:**

```qd
parts count "/" strings::join!  // path
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
"hello" strings::len print  // 5
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
"hello hello" "hello" strings::last_index_of print  // 6
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
"HELLO" strings::lower print  // "hello"
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
"ab" 3 strings::repeat print  // "ababab"
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
"hello" strings::reverse print  // "olleh"
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
| `strings::ErrAlloc` | Memory allocation failed |

**Example:**

```qd
"hello" "l" "L" strings::replace! print  // "heLLo"
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
entries count strings::sort
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
entries count strings::sort_desc
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
| `strings::ErrAlloc` | Memory allocation failed |

**Example:**

```qd
"a,b,c" "," strings::split!  // parts=["a","b","c"], count=3
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
"hello" "hel" strings::starts_with print  // 1
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
| `strings::ErrOutOfBounds` | Index out of bounds |

**Example:**

```qd
"hello" 1 3 strings::substring! print  // "ell"
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
"  hello  " strings::trim print  // "hello"
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
"  hello  " strings::trim_left print  // "hello  "
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
"  hello  " strings::trim_right print  // "  hello"
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
"hello" strings::upper print  // "HELLO"
```
---

### `fn` is_empty

Check if string is empty.

**Signature:** `(s:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if empty, 0 otherwise |

**Example:**

```qd
"" strings::is_empty print     // 1
"hello" strings::is_empty print // 0
```
---

### `fn` is_blank

Check if string is empty or contains only whitespace.

**Signature:** `(s:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if blank, 0 otherwise |

**Example:**

```qd
"   " strings::is_blank print  // 1
"hi" strings::is_blank print   // 0
```
---

### `fn` equals_ignore_case

Case-insensitive string comparison.

**Signature:** `(a:str b:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | `str` | First string |
| `b` | `str` | Second string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if equal ignoring case, 0 otherwise |

**Example:**

```qd
"Hello" "HELLO" strings::equals_ignore_case print  // 1
```
---

### `fn` pad_left

Left-pad string to target length.

**Signature:** `(s:str len:i64 ch:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |
| `len` | `i64` | Target length |
| `ch` | `str` | Padding character |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Padded string |

**Example:**

```qd
"42" 5 "0" strings::pad_left print  // "00042"
```
---

### `fn` pad_right

Right-pad string to target length.

**Signature:** `(s:str len:i64 ch:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |
| `len` | `i64` | Target length |
| `ch` | `str` | Padding character |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Padded string |

**Example:**

```qd
"hi" 5 "." strings::pad_right print  // "hi..."
```
---

### `fn` center

Center string with padding on both sides.

**Signature:** `(s:str len:i64 ch:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |
| `len` | `i64` | Target length |
| `ch` | `str` | Padding character |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Centered string |

**Example:**

```qd
"hi" 6 "-" strings::center print  // "--hi--"
```
---

### `fn` capitalize

Capitalize first character, lowercase the rest.

**Signature:** `(s:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Capitalized string |

**Example:**

```qd
"hELLO" strings::capitalize print  // "Hello"
```
---

### `fn` title

Title case - capitalize first letter of each word.

**Signature:** `(s:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Title-cased string |

**Example:**

```qd
"hello world" strings::title print  // "Hello World"
```
---

### `fn` trim_prefix

Remove prefix if present.

**Signature:** `(s:str prefix:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |
| `prefix` | `str` | Prefix to remove |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | String without prefix |

**Example:**

```qd
"hello" "hel" strings::trim_prefix print  // "lo"
```
---

### `fn` trim_suffix

Remove suffix if present.

**Signature:** `(s:str suffix:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |
| `suffix` | `str` | Suffix to remove |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | String without suffix |

**Example:**

```qd
"hello" "lo" strings::trim_suffix print  // "hel"
```
---

### `fn` replace_first

Replace first occurrence only.

**Signature:** `(s:str old:str new:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Source string |
| `old` | `str` | Substring to replace |
| `new` | `str` | Replacement string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | String with first occurrence replaced |

**Example:**

```qd
"abab" "ab" "X" strings::replace_first print  // "Xab"
```
---

### `fn` insert

Insert string at position.

**Signature:** `(s:str pos:i64 ins:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Original string |
| `pos` | `i64` | Insert position |
| `ins` | `str` | String to insert |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | String with insertion |

**Example:**

```qd
"hello" 2 "XX" strings::insert print  // "heXXllo"
```
---

### `fn` remove_range

Remove range from string.

**Signature:** `(s:str start:i64 len:i64 -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |
| `start` | `i64` | Start position |
| `len` | `i64` | Characters to remove |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | String with range removed |

**Example:**

```qd
"hello" 1 3 strings::remove_range print  // "ho"
```
---

### `fn` truncate

Truncate to max length with optional suffix.

**Signature:** `(s:str max:i64 suffix:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |
| `max` | `i64` | Maximum length |
| `suffix` | `str` | Suffix when truncated (e.g. "...") |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Truncated string |

**Example:**

```qd
"hello world" 8 "..." strings::truncate print  // "hello..."
```
---

### `fn` lines

Split string by newlines.

**Signature:** `(s:str -- arr:ptr count:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `arr` | `ptr` | Array of line strings |
| `count` | `i64` | Number of lines |

**Example:**

```qd
"a\nb\nc" strings::lines! -> count -> arr
// arr contains ["a", "b", "c"], count = 3
```
---

### `fn` words

Split string by whitespace.

**Signature:** `(s:str -- arr:ptr count:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `arr` | `ptr` | Array of word strings |
| `count` | `i64` | Number of words |

**Example:**

```qd
"hello  world" strings::words! -> count -> arr
// arr contains ["hello", "world"], count = 2
```
---

### `fn` split_n

Split into at most n parts.

**Signature:** `(s:str delim:str n:i64 -- parts:ptr count:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | String to split |
| `delim` | `str` | Delimiter |
| `n` | `i64` | Maximum number of parts |

| Output | Type | Description |
|--------|------|-------------|
| `parts` | `ptr` | Array of string parts |
| `count` | `i64` | Number of parts |

**Example:**

```qd
"a:b:c:d" ":" 2 strings::split_n! -> count -> arr
// arr contains ["a", "b:c:d"], count = 2
```
---

### `fn` is_numeric

Check if string contains only digits.

**Signature:** `(s:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if numeric, 0 otherwise |

**Example:**

```qd
"12345" strings::is_numeric print  // 1
"12.34" strings::is_numeric print  // 0
```
---

### `fn` is_alpha

Check if string contains only letters.

**Signature:** `(s:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if alphabetic, 0 otherwise |

**Example:**

```qd
"Hello" strings::is_alpha print    // 1
"Hello123" strings::is_alpha print // 0
```
---

### `fn` is_alphanumeric

Check if string contains only letters and digits.

**Signature:** `(s:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if alphanumeric, 0 otherwise |

**Example:**

```qd
"Hello123" strings::is_alphanumeric print  // 1
"Hello!" strings::is_alphanumeric print    // 0
```
---

### `fn` is_ascii

Check if all characters are ASCII (0-127).

**Signature:** `(s:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if all ASCII, 0 otherwise |

**Example:**

```qd
"hello" strings::is_ascii print  // 1
```
---

### `fn` is_lowercase

Check if all letters are lowercase.

**Signature:** `(s:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if lowercase, 0 otherwise |

**Example:**

```qd
"hello" strings::is_lowercase print  // 1
"Hello" strings::is_lowercase print  // 0
```
---

### `fn` is_uppercase

Check if all letters are uppercase.

**Signature:** `(s:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if uppercase, 0 otherwise |

**Example:**

```qd
"HELLO" strings::is_uppercase print  // 1
"Hello" strings::is_uppercase print  // 0
```
---

### `fn` char_count

Count UTF-8 codepoints (characters, not bytes).

**Signature:** `(s:str -- count:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |

| Output | Type | Description |
|--------|------|-------------|
| `count` | `i64` | Number of UTF-8 codepoints |

**Example:**

```qd
"hello" strings::char_count print  // 5
"héllo" strings::char_count print  // 5 (é is 2 bytes but 1 char)
```
---

### `fn` slice

Substring with negative index support (Python-style).

**Signature:** `(s:str start:i64 end:i64 -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `s` | `str` | Input string |
| `start` | `i64` | Start index (negative counts from end) |
| `end` | `i64` | End index (negative counts from end) |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Sliced substring |

**Example:**

```qd
"hello" 1 4 strings::slice print   // "ell"
"hello" -3 -1 strings::slice print // "ll"
"hello" 1 -1 strings::slice print  // "ell"
```
---

### `fn` column

Format strings into columns.

**Signature:** `(arr:ptr count:i64 widths:ptr num_cols:i64 -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `arr` | `ptr` | Array of strings |
| `count` | `i64` | Number of strings |
| `widths` | `ptr` | Array of column widths |
| `num_cols` | `i64` | Number of columns |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Formatted column output |

**Example:**

```qd
// Format names into 2 columns of width 10
names count widths 2 strings::column print
```
