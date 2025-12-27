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
