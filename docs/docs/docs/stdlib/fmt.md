# fmt

Formatted output functions.

## Format Specifiers

Both `printf` and `sprintf` support the following format specifiers:

- `%s` - String
- `%d`, `%i` - Integer
- `%f` - Float
- `%%` - Literal % character (no argument consumed)

Arguments are pushed onto the stack in the order they appear in the format string, with the format string pushed last.

## Functions

### printf

Print formatted output to stdout.

**Signature:** `( arg1 arg2 ... argN format:str -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `format` | `str` | Format string with % specifiers |

**Example:**

```qd
"world" "Hello %s!\n" fmt::printf  // Output: Hello world!
"Alice" 42 "%s scored %d points\n" fmt::printf  // Output: Alice scored 42 points
```

### sprintf

Format a string and push the result onto the stack.

**Signature:** `( arg1 arg2 ... argN format:str -- result:str )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `format` | `str` | Format string with % specifiers |

| Return | Type | Description |
|--------|------|-------------|
| `result` | `str` | Formatted string |

**Example:**

```qd
"world" "Hello %s!" fmt::sprintf  // Stack: "Hello world!"
"Alice" 42 "%s scored %d points" fmt::sprintf  // Stack: "Alice scored 42 points"

// Chain with other operations
"test" 100 "%s: %d" fmt::sprintf str::len print nl  // Output: 9
```
