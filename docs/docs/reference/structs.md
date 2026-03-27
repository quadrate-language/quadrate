# Structs

Custom data types that group related fields together.

## Overview

| Syntax | Description |
|--------|-------------|
| `struct Name { fields }` | Define a struct |
| `pub struct Name { fields }` | Define a public struct (visible to other modules) |
| `struct Name<T> { fields }` | Define a generic struct |
| `Name { field = value }` | Create an instance |
| `<<field` | Read a field |
| `struct value >>field` | Write a field (pushes modified struct back for chaining) |
| `struct value >>field!` | Write a field (discards struct, for standalone mutation) |
| `fn (s:Name) method(...)` | Define a method |

---

## Defining structs

```qd
struct Point {
	x:f64
	y:f64
}
```

Field types can be any valid type: `i64`, `f64`, `str`, `ptr`, another struct, or a pointer to a struct (`*StructName`).

### Default values

Fields can have default values:

```qd
struct Config {
	debug:i64 = 0
	verbose:i64 = 0
	max_retries:i64 = 3
}

fn main() {
	Config {} -> cfg           // all defaults
	cfg <<max_retries print nl  // 3
}
```

### Public structs

Use `pub` to export a struct from a module:

```qd
pub struct Vec2 {
	x:f64
	y:f64
}
```

---

## Creating instances

Push field values with `field = expr` inside braces:

```qd
Point { x = 3.0 y = 4.0 } -> p
```

All fields without defaults must be provided. Order does not matter.

---

## Field access (read)

Use `<<fieldname` to read a field value. The struct is consumed from the stack and the field value is pushed. The `<<` indicates data flows left (out of the struct).

**Syntax:** `struct <<field` → pushes field value

```qd
p <<x print nl  // read x from p
```

### Chained access

For nested structs, chain `<<` operators:

```qd
struct Line {
	start:Point
	end:Point
}

line <<start <<x print nl  // read x from start point of line
```

### Type narrowing for ptr values

When a value is typed as `ptr` (e.g., in callback handlers), use `as` to tell the compiler which struct type it is:

```qd
fn handler(c:ptr -- ) {
	c as http::Ctx <<body -> body
}
```

If multiple structs share a field name and the compiler cannot determine the type, accessing `<<field` on an untyped `ptr` is a compile error. Use `as StructType` to disambiguate.

See [Type Casting](types.md#type-narrowing-with-as) for details.

---

## Field set (write)

Use `>>fieldname` to write a field value. The struct must be on the stack, followed by the value. The `>>` indicates data flows right (into the struct). The modified struct is pushed back onto the stack.

**Syntax:** `struct value >>field` — returns modified struct (for chaining)

```qd
// Chaining — struct stays on stack
Point { x = 0.0 y = 0.0 } 1.0 >>x 2.0 >>y -> p

// Rebind after write
p 5.0 >>x -> p
```

Use `>>field!` (with `!`) when you don't need the struct back — for standalone mutations:

**Syntax:** `struct value >>field!` — discards struct after write

```qd
p 99 >>x!           // mutate, don't push struct back
s sz 1 + >>size!    // common in imperative code
```

---

## Methods

Methods are functions with a receiver parameter. The receiver uses a special syntax before the function name.

**Syntax:** `fn (receiver:StructType) name(params -- returns) { body }`

```qd
struct Circle {
	radius:f64
}

fn (c:Circle) area( -- a:f64) {
	c <<radius dup * 3.14159 *
}

fn main() {
	Circle { radius = 5.0 } -> c
	c area print nl  // 78.5397
}
```

### Method calls

When a struct is on the stack and you call a function name, the compiler checks for a matching method on that struct type first:

```qd
c area       // calls (c:Circle) area
```

The struct is consumed from the stack as the receiver.

### Methods with parameters

```qd
struct Point {
	x:f64
	y:f64
}

fn (p:Point) translate(dx:f64 dy:f64 -- result:Point) {
	p p <<x dx + >>x p <<y dy + >>y
}

fn main() {
	Point { x = 1.0 y = 2.0 } -> p
	p 3.0 4.0 translate -> p
	p <<x print nl  // 4
	p <<y print nl  // 6
}
```

### Method resolution

Methods take precedence over global functions with the same name:

```qd
struct Counter {
	value:i64
}

fn (c:Counter) bump( -- result:i64) {
	c <<value 1 +
}

fn bump( -- result:i64) {
	999
}

fn main() {
	Counter { value = 10 } -> c
	c bump print nl  // 11 (method)
	bump print nl    // 999 (global)
}
```

---

## Generic structs

Structs can have type parameters:

```qd
struct Box<T> {
	value:T
}

fn main() {
	Box<i64> { value = 42 } -> b
	b <<value print nl  // 42
}
```

Multiple type parameters:

```qd
struct Pair<A, B> {
	first:A
	second:B
}
```

**Note:** Generic structs currently work best with `i64` fields. Using `f64` or `str` type parameters may return raw bits or pointers.

---

## See also

- [Structs](../learn/5-data-structures/structs.md) in the Learn section
- [Generics](generics.md) for generic struct details
