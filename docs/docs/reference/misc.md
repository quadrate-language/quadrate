# Miscellaneous

Other built-in operations.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `call` | `(fn -- ...)` | Run a function value (the simplest combinator) |
| `sizeof<T>` | `( -- n:i64)` | Size of type T in bytes (compile-time) |

---

## sizeof

Get the size of a type in bytes at compile time.

**Signature:** `( -- n:i64)`

```qd
struct Point { x:i64 y:i64 }

fn main() {
	sizeof<i64> print nl    // 8
	sizeof<f64> print nl    // 8
	sizeof<Point> print nl  // 16
}
```

Works with primitive types (`i64`, `f64`, `str`, `ptr`) and struct types. Useful for memory allocation with `mem::alloc`.

---

## Function values

### call

Runs a function value: a `fn { }` literal, a local holding one, or `&name`. It is the simplest
of the combinators, which run function values in the patterns stack code repeats.

**Signature:** `(fn -- ...)`

```qd
fn double(x:i64 -- result:i64) {
	2 *
}

fn main() {
	5 &double call print nl  // 10
}
```

---

## Getting function values

`&name` is a function value for a named function. It is static: nothing is allocated and
nothing captured. A `fn { }` literal is a function value too.

```qd
&myfunction -> f
```

`ptr` is raw memory and is reserved as a type name, so it cannot name the local.

## Calling function values

Push the arguments, then the function value, then `call`:

```qd
// For: fn addition(a:i64 b:i64 -- sum:i64)
3 4 &addition call print nl  // 7
```

---

## Common uses

### Callbacks

```qd
fn for_each(arr:[]i64 callback:fn(i64 --) -- ) {
	-> callback -> arr
	0 arr len 1 for i {
		arr i nth callback call
	}
}

fn print_item(x:i64 -- ) {
	print " " print
}

fn main() {
	[1 2 3 4 5] &print_item for_each
	nl
}
```

### Function tables

```qd
fn op_add(a:i64 b:i64 -- r:i64) {
	+
}

fn op_sub(a:i64 b:i64 -- r:i64) {
	-
}

fn op_mul(a:i64 b:i64 -- r:i64) {
	*
}

fn main() {
	[3]fn(i64 i64 -- i64) -> ops
	ops 0 &op_add set
	ops 1 &op_sub set
	ops 2 &op_mul set

	10 5 ops 0 nth call print nl  // 15
	10 5 ops 1 nth call print nl  // 5
	10 5 ops 2 nth call print nl  // 50
}
```

### Higher-order functions

```qd
fn map_ints(arr:[]i64 f:fn(i64 -- i64) -- result:[]i64) {
	-> f -> arr
	[arr len]i64 -> result
	0 arr len 1 for i {
		result i arr i nth f call set
	}
	result
}

fn square(x:i64 -- r:i64) { dup * }

fn main() {
	[1 2 3 4 5] &square map_ints -> squared
	0 squared len 1 for i {
		squared i nth print " " print
	}
	nl  // 1 4 9 16 25
}
```

---

## See also

- [Function values](../learn/7-advanced/function-pointers.md) in the Learn section
