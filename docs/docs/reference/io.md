# Input/Output

Built-in operations for input and output.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `print` | `(val --)` | Print value |
| `dump` | `(val --)` | Print with type info |
| `nl` | `(--)` | Print newline |

---

## Output

### print

Prints a value to stdout without a newline.

**Signature:** `(val --)`

```qd
42 print // prints: 42
"hello" print // prints: hello
```

### dump

Prints a value with type information for debugging.

**Signature:** `(val --)`

```qd
42 dump // prints: int:42
```

### nl

Prints a newline character to stdout.

**Signature:** `(--)`

```qd
"Hello" print nl // prints: Hello\n
```

---

## Input

Command-line arguments are not a builtin. `os::args` returns them as a string array,
excluding the program name, and `os::program_name` gives argv[0]:

```qd
use os

fn main() {
	os::args -> args
	args len print nl
	args free
}
```

---

## Common patterns

### Print with newline

```qd
42 print nl
```

### Print multiple values

```qd
"x = " print x print nl
```

### String interpolation

Use `$"..."` to embed expressions directly in strings:

```qd
"World" -> name
42 -> age
$"Hello, {name}! Age: {age}" print nl
```

Expressions inside `{...}` are evaluated and converted to strings automatically. The `sb` module is auto-imported when `$"..."` is used.

### Debug output

```qd
"Debug: " print
dup dump // Show the top value and its type, leaving it
```

---

## File I/O

For file operations (open, read, write, etc.), use the `io` module from the standard library.

See the [io module](../stdlib/io.md) documentation for details.
