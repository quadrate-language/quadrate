# regex

Regular expression matching using Thompson NFA.
Supports = . * + ? | () [] [^] [a-z] ^ it and escapes.
Note: Nested groups and alternation inside groups not yet supported.

## Functions

### compile

Compile a regex pattern.

**Signature:** `( pattern:str -- re:ptr )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `pattern` | `str` | Regular expression pattern |

| Return | Type | Description |
|--------|------|-------------|
| `re` | `ptr` | Compiled regex (null on error) |

---

### matches

Test if a string matches a compiled regex (full match).

**Signature:** `( re:ptr s:str -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `re` | `ptr` | Compiled regex |
| `s` | `str` | String to match |

| Return | Type | Description |
|--------|------|-------------|
| `matches` | `i64` | 1 if matches, 0 otherwise |

---

### release

Free a compiled regex.

**Signature:** `( re:ptr -- )`

---

### test

Compile and test if pattern matches string.

**Signature:** `( pattern:str s:str -- result:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `pattern` | `str` | Regex pattern |
| `s` | `str` | String to match |

| Return | Type | Description |
|--------|------|-------------|
| `matches` | `i64` | 1 if matches, 0 otherwise |
