# Keywords

Language keywords for declarations, control flow, and more.

## Overview

| Keyword | Description |
|---------|-------------|
| [`fn`](#fn) | Declares a function with a stack signature |
| [`pub`](#pub) | Makes a function, constant, or struct visible to other modules |
| [`const`](#const) | Declares a compile-time constant value |
| [`struct`](#struct) | Declares a structured data type with named fields |
| [`use`](#use) | Imports a module |
| [`if`](#if) | Conditional execution |
| [`else`](#else) | Alternative block when if condition is false |
| [`for`](#for) | Iteration with counter |
| [`loop`](#loop) | Infinite loop |
| [`break`](#break) | Exit loop |
| [`continue`](#continue) | Skip to next iteration |
| [`return`](#return) | Exit function |
| [`defer`](#defer) | Schedule cleanup code |
| [`switch`](#switch) | Multi-way branching |
| [`=>`](#case-arrow) | Case separator in switch |
| [`default`](#default) | Fallback case in switch |
| [`ctx`](#ctx) | Context variable access |
| [`->`](#arrow) | Variable binding |
| [`true`](#true) | Boolean true (1) |
| [`false`](#false) | Boolean false (0) |

---

## Declarations

### fn

Declares a function with a stack signature.

```qd
fn add(a:i64 b:i64 -- sum:i64) { + }
```

### pub

Makes a function, constant, or struct visible to other modules.

```qd
pub fn greet( -- ) { "Hello" print nl }
```

### const

Declares a compile-time constant value.

```qd
const PI 3.14159
```

### struct

Declares a structured data type with named fields.

```qd
struct Point { x:f64 y:f64 }
```

### use

Imports a module, making its functions available with `module::function` syntax.

```qd
use str
```

---

## Control Flow

### if

Executes a block if the top of stack is true (non-zero).

```qd
5 3 > if { "yes" print }
```

### else

Provides an alternative block when the if condition is false.

```qd
x 0 > if { "positive" } else { "non-positive" }
```

### for

Iterates from start to end with a step, binding the iterator variable.

```qd
0 10 1 for i { i print nl }
```

### loop

Repeats a block indefinitely until break is called.

```qd
loop { "forever" print nl }
```

### break

Exits the innermost loop immediately.

```qd
loop { x 10 > if { break } }
```

### continue

Skips to the next iteration of the innermost loop.

```qd
0 10 1 for i { i 5 == if { continue } i print nl }
```

### return

Exits the current function immediately.

```qd
fn early( -- ) { true if { return } "not reached" print }
```

### switch

Branches based on matching the top of stack against case values.

```qd
x switch { 1 => { "one" } 2 => { "two" } default => { "other" } }
```

### =>

Separates a case value from its block in a switch statement.

```qd
1 => { "one" print }
```

### default

Provides a fallback block when no switch case matches.

```qd
default => { "no match" print }
```

---

## Other Keywords

### defer

Schedules a block to run when the function exits, in LIFO order.

```qd
defer { file io::close }
```

### ctx

Accesses a context variable.

```qd
"username" ctx -> name
```

### ->

Pops a value from the stack and binds it to a local variable.

```qd
42 -> x
```

### true

Pushes 1 onto the stack.

```qd
true if { "yes" print }
```

### false

Pushes 0 onto the stack.

```qd
false if { } else { "no" print }
```
