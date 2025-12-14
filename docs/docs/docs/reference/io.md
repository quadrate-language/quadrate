# Input/Output

Built-in operations for input and output.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `print` | `(val --)` | Print value |
| `printv` | `(val --)` | Print with type info |
| `prints` | `()` | Print entire stack |
| `printsv` | `()` | Print stack with types |
| `nl` | `()` | Print newline |
| `read` | `(-- ... n)` | Read command line args |

---

## Output

### print

Prints a value to stdout without a newline.

**Signature:** `(val --)`

```qd
42 print // prints: 42
"hello" print // prints: hello
```

### printv

Prints a value with type information for debugging.

**Signature:** `(val --)`

```qd
42 printv // prints something like: i64:42
```

### prints

Prints the entire stack contents without clearing it.

**Signature:** `()`

```qd
1 2 3 prints // prints the entire stack
```

Useful for debugging to see current stack state.

### printsv

Prints the entire stack with type information for debugging.

**Signature:** `()`

Similar to `prints` but includes type info for each value.

### nl

Prints a newline character to stdout.

**Signature:** `()`

```qd
"Hello" print nl // prints: Hello\n
```

---

## Input

### read

Reads command line arguments onto the stack, pushing count last.

**Signature:** `(-- ... n)`

```qd
read -> argc // argc is the number of arguments
```

Arguments are pushed onto the stack with the count on top.

---

## Common Patterns

### Print with newline

```qd
42 print nl
```

### Print multiple values

```qd
"x = " print x print nl
```

### Debug output

```qd
"Debug: " print
prints // Show stack
nl
```

---

## File I/O

For file operations (open, read, write, etc.), use the `io` module from the standard library.

See the [io module](../stdlib/io.md) documentation for details.
