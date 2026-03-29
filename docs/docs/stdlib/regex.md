# `use` regex

Regular expression matching using Thompson NFA.
Supports . * + ? | () [] [^] [a-z] ^ $ and escapes.
Note: Nested groups and alternation inside groups not yet supported.

## Functions

### `fn` compile

Compile a regex pattern.

**Signature:** `(pattern:str -- re:ptr)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `pattern` | `str` | Regular expression pattern |

| Output | Type | Description |
|--------|------|-------------|
| `re` | `ptr` | Compiled regex (null on error) |
---

### `fn` find_all

Find all non-overlapping matches. Returns array of start/end pairs and count. Caller must free the returned array with mem::free.

**Signature:** `(re:ptr s:str max:i64 -- results:ptr count:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `re` | `ptr` | Compiled regex |
| `s` | `str` | String to search |
| `max` | `i64` | Maximum matches to find (-1 for unlimited) |

| Output | Type | Description |
|--------|------|-------------|
| `results` | `ptr` | Array of i64 pairs [start0, end0, start1, end1, ...] |
| `count` | `i64` | Number of matches found |

**Example:**

```qd
"[0-9]+" regex::compile -> re  re "a1b22c333" -1 regex::find_all -> results  // count
```
---

### `fn` find

Find first match in string. Returns start and end (exclusive) indices. Returns -1 -1 if no match found.

**Signature:** `(re:ptr s:str -- start:i64 end:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `re` | `ptr` | Compiled regex |
| `s` | `str` | String to search |

| Output | Type | Description |
|--------|------|-------------|
| `start` | `i64` | Start index of match (-1 if none) |
| `end` | `i64` | End index of match, exclusive (-1 if none) |

**Example:**

```qd
"[0-9]+" regex::compile -> re  re "abc123def" regex::find -> start  // end
```
---

### `fn` is_match

Compile and test if pattern matches string.

**Signature:** `(pattern:str s:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `pattern` | `str` | Regex pattern |
| `s` | `str` | String to match |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `i64` | 1 if matches, 0 otherwise |

**Example:**

```qd
"hello.*" "hello world" regex::is_match  // result
```
---

### `fn` match_count

Count non-overlapping matches in string.

**Signature:** `(re:ptr s:str -- count:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `re` | `ptr` | Compiled regex |
| `s` | `str` | String to search |

| Output | Type | Description |
|--------|------|-------------|
| `count` | `i64` | Number of matches |

**Example:**

```qd
"[0-9]+" regex::compile -> re  re "a1b22c333" regex::match_count  // n
```
---

### `fn` matches

Test if a string matches a compiled regex (full match).

**Signature:** `(re:ptr s:str -- result:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `re` | `ptr` | Compiled regex |
| `s` | `str` | String to match |

| Output | Type | Description |
|--------|------|-------------|
| `matches` | `i64` | 1 if matches, 0 otherwise |
---

### `fn` release

Free a compiled regex.

**Signature:** `(re:ptr -- )`
---

### `fn` replace

Replace all matches with replacement string.

**Signature:** `(re:ptr s:str repl:str -- result:str)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `re` | `ptr` | Compiled regex |
| `s` | `str` | Input string |
| `repl` | `str` | Replacement string |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | String with all matches replaced |

**Example:**

```qd
"[0-9]+" regex::compile -> re  re "abc123def456" "#" regex::replace  // result
```
