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
| [`while`](#while) | Conditional loop |
| [`loop`](#loop) | Infinite loop |
| [`break`](#break) | Exit loop |
| [`continue`](#continue) | Skip to next iteration |
| [`return`](#return) | Exit function |
| [`defer`](#defer) | Schedule cleanup code |
| [`switch`](#switch) | Multi-way branching |
| [`ctx`](#ctx) | Context variable access |
| [`->`](#arrow-) | Variable binding |
| [`test`](#test) | Declares a test block |
| [`import`](#import) | Imports a native C library |
| [`as`](#as) | Specifies module name in import |
| [`Ok`](#ok) | Success literal (1) for switch matching |
| [`Err`](#err) | Error literal (0) for switch matching |
| [`true`](#true) | Boolean true (1) |
| [`false`](#false) | Boolean false (0) |

---

## Declarations

### fn

Declares a function with a stack signature.

```qd
fn add(a:i64 b:i64 -- sum:i64) {
	+
}
```

### pub

Makes a function, constant, or struct visible to other modules.

```qd
pub fn greet() {
	"Hello" print nl
}
```

### const

Declares a compile-time constant value.

```qd
const Pi = 3.14159
```

### struct

Declares a structured data type with named fields.

```qd
struct Point {
	x:f64
	y:f64
}
```

### use

Imports a module, making its functions available with `module::function` syntax.

```qd
use strings

fn main() {
	"hello" strings::len print nl  // 5
}
```

---

## Control flow

### if

Executes a block if the top of stack is true (non-zero).

```qd
5 3 > if {
	"yes" print
}
```

### else

Provides an alternative block when the if condition is false.

```qd
x 0 > if {
	"positive"
} else {
	"non-positive"
}
```

### for

Iterates from start to end with a step, binding the iterator variable.

```qd
0 10 1 for i {
	i print nl
}
```

### while

Repeats a block while the condition is true. The condition is evaluated before entering and at the end of each iteration.

```qd
0 -> i
i 5 < while {
	i print nl
	i 1 + -> i
	i 5 <          // condition for next iteration
}
```

### loop

Repeats a block indefinitely until break is called.

```qd
loop {
	"forever"
	print nl
}
```

### break

Exits the innermost loop immediately.

```qd
loop {
	x 10 > if {
		break
	}
}
```

### continue

Skips to the next iteration of the innermost loop.

```qd
0 10 1 for i {
	i 5 == if {
		continue
	}
	i print nl
}
```

### return

Exits the current function immediately.

```qd
fn do_work( -- ) {
	"working" print nl
	return
	"not reached" print
}
```

!!! note
    `return` only works at the function body's top level. It cannot be used inside `if`, `else`, `loop`, or other blocks.

### switch

Branches based on matching the top of stack against case values. Use `_` as the default case when no other case matches.

```qd
switch {
	1 {
		"one"
	}
	2 {
		"two"
	}
	_ {
		"other"
	}
}
```

---

## Other keywords

### defer

Schedules a block to run when the function exits, in LIFO order.

```qd
defer {
	file io::close
}
```

### ctx

Creates a new isolated stack context. The child starts with a copy of the parent stack but cannot modify parent variables.

```qd
ctx {
	// child context
}
```

### test

Declares a test block. Tests are run with `quad test` or `quadc --test`.

```qd
use testing

test "addition works" {
	1 2 + 3 testing::assert_eq
}
```

### import

Imports a native C library for use in a module. Used with `as` to specify the module namespace.

```qd
import "libqdmath.a" as "math" {
	pub fn sqrt(x:f64 -- result:f64)
}
```

### as

Specifies the module namespace in an `import` statement. See [import](#import).

### Ok

Literal `1` (same as `true`). Used in `switch` to match the success case after a fallible function call.

```qd
"test.txt" io::read_file switch {
	Ok { -> content content print }
	_ { "error" print nl }
}
```

### Err

Literal `0` (same as `false`). Used in `switch` to match the error case after a fallible function call.

```qd
"test.txt" io::read_file switch {
	Ok { -> content content print }
	Err { "error reading file" print nl }
}
```

### arrow (->)

Pops a value from the stack and binds it to a local variable.

```qd
42 -> x
```

### true

Pushes 1 onto the stack.

```qd
true if {
	"yes" print
}
```

### false

Pushes 0 onto the stack.

```qd
false if {
} else {
	"no" print
}
```
