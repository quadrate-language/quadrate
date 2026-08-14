# Keywords

Language keywords for declarations, control flow, and more.

## Overview

| Keyword | Description |
|---------|-------------|
| [`fn`](#fn) | Declares a function with a stack signature |
| [`pub`](#pub) | Makes a function, constant, or struct visible to other modules |
| [`inline`](#inline) | Requests the compiler to inline a function at call sites |
| [`const`](#const) | Declares a compile-time constant value |
| [`var`](#var) | Declares a module-level mutable global variable |
| [`struct`](#struct) | Declares a structured data type with named fields |
| [`packed`](#packed) | Struct modifier: fields laid out adjacent with no padding |
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
| [`ctx`](#ctx) | Context variable access |
| [`->`](#arrow-) | Variable binding |
| [`test`](#test) | Declares a test block |
| [`import`](#import) | Imports a native C library |
| [`enum`](#enum) | Declares an enumeration type |
| [`type`](#type) | Declares a type alias |
| [`as`](#as) | Type narrowing cast / import module namespace |
| [`null`](#null) | Null pointer literal (0) |
| [`$"..."`](#string-interpolation) | String interpolation |
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

### inline

Requests the compiler to inline a function at call sites, eliminating function call overhead. Can be combined with `pub`.

The compiler will inline in most cases, but may decline for recursive functions or indirect calls through function pointers. Best suited for small wrapper functions where the call overhead would dominate the function body.

```qd
inline fn double(x:i64 -- result:i64) {
	x 2 *
}

pub inline fn inc(x:i64 -- result:i64) {
	x 1 +
}
```

### const

Declares a compile-time constant value.

```qd
const Pi = 3.14159
```

### var

Declares a module-level mutable global variable. The initializer is required; the `:type` annotation is optional — when omitted, the type is inferred from the initializer. Reads use the bare name; writes use `->`. Unlike `const`, the value can change across function calls. Use `pub var` to export.

```qd
var counter = 0           // inferred i64
var ratio = 3.14          // inferred f64
var label = "hello"       // inferred str
pub var magic:i64 = 42    // explicit type still supported

fn bump() {
	counter 1 + -> counter
}
```

**Inference rules:** integer literal → `i64`, float literal → `f64`, string literal → `str`, const-reference → the const's type (derived from its value shape). Write `:type` explicitly when you want a narrower type (e.g. `var port:u16 = 8080`) or when the inferred default isn't what you want — the declared type becomes metadata for field/memory-boundary operations; the global's storage is still `i64`-wide for integer kinds.

Supported types: `i64`, `f64`, `str` (always null-initialized), `ptr`, any struct type, plus all sized ints (`i8/i16/i32/u8/u16/u32/u64`) as explicit annotations. Initializers accept a literal, a reference to a previously-declared `const`, or a struct construction (`var p = Point { x = 1.0 y = 2.0 }`). Struct initializers run once before `main` and are stored as a reference-counted pointer in the global. A `-> name` assignment inside any function always writes to the matching module-level var — locals never shadow globals; pick a different name if you need a local.


### struct

Declares a structured data type with named fields.

```qd
struct Point {
	x:f64
	y:f64
}
```

### packed

Struct modifier: fields are laid out back-to-back at their exact widths with no padding between them. Use for parsing on-disk binary formats (WAD lumps, PNG chunks, network protocol headers) where the memory layout must match a byte-exact specification. Combines with `pub`: `pub packed struct ...`.

```qd
packed struct FileLump {
	offset:u32
	size:u32
	name:u64       // 8 raw bytes
}
// sizeof = 16 bytes; a plain `struct` would round each field to 8 bytes for 24.
```

Without `packed`, each field is rounded up to an 8-byte slot for uniform alignment. With `packed`, `struct FileLump` above is exactly 16 bytes wide.

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

`return` exits the enclosing function from anywhere — including inside `if`, `else`, `for`,
`loop` and `switch` arms — which makes the guard-clause style available:

```qd
fn classify(n:i64 -- label:str) {
	n 0 < if {
		"negative"
		return
	}
	n 0 == if {
		"zero"
		return
	}
	"positive"
}

fn main() {
	-5 classify print nl
	0 classify print nl
	7 classify print nl
}
```

A function that declares outputs must leave them on the stack before returning, on every path
that returns. Deferred blocks registered before the `return` still run (see [defer](#defer)).

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
import "libmath.a" as "math" {
	pub fn sqrt(x:f64 -- result:f64)
}
```

### as

Has two uses:

**1. Type narrowing cast** — Narrows a `ptr` value to a specific struct type for field access. This is compile-time only with no runtime cost.

```qd
fn get_body(c:ptr -- body:str) {
	c as http::Ctx <<body
}
```

When multiple structs share a field name and the compiler cannot determine the type, `as` disambiguates:

```qd
c as MyStruct <<value      // access <<value on MyStruct specifically
c as MyStruct -> typed_c  // bind as typed local for subsequent accesses
typed_c <<value            // type is known, no ambiguity
```

**2. Import namespace** — Specifies the module namespace in an `import` statement. See [import](#import).

### enum

Declares an enumeration with named integer constants. Values auto-increment from 0 unless explicitly assigned.

```qd
enum Color { Red Green Blue }
enum Token { Int Float Str = 10 Ident }
```

Access variants with `EnumName::Variant`. Use `pub enum` to export from a module.

```qd
Color::Red print nl     // 0
Color::Blue print nl    // 2
```

### type

Declares a type alias. The alias is resolved at compile time with no runtime cost.

```qd
type Predicate = fn(i64 -- i64)
type IntArray = []i64
type Number = i64
```

Use `pub type` to export from a module.

```qd
fn filter(arr:IntArray pred:Predicate -- result:IntArray) { ... }
```

### null

Pushes 0 with pointer semantics. Used to represent the absence of a value in pointer fields.

```qd
struct Node { value:i64 next:*Node }
Node { value = 10 next = null } -> n
null 0 == if { "true" print nl }   // true
```

### String interpolation

The `$"..."` syntax embeds expressions in strings. Expressions inside `{...}` are evaluated and converted to strings.

```qd
"World" -> name
42 -> age
$"Hello, {name}! Age: {age}" print nl
```

Desugars to `sb::new` / `sb::append` / `sb::finish` calls. The `sb` module is auto-imported.

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
