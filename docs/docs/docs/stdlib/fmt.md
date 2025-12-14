# fmt

Formatted output functions.

## Functions

### printf

Print formatted output to stdout.

**Signature:** `( format:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `format` | `str` | Format string with % specifiers |

**Example:**

```qd
"world" "Hello %s\n" fmt::printf  // Hello world
```

---

### sprintf

Format a string with printf-style specifiers.

**Signature:** `( format:str -- result:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `format` | `str` | Format string with % specifiers |

| Output | Type | Description |
|--------|------|-------------|
| `result` | `str` | Formatted string |

**Example:**

```qd
"world" "Hello %s\n" fmt::sprintf  // "Hello world\n"
```
