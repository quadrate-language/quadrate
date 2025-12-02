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
"Hello %s\n" "world" fmt::printf  // Hello world
```
