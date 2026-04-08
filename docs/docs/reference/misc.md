# Miscellaneous

Other built-in operations.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `call` | `(fn -- ...)` | Call a function pointer |
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

## Function pointers

### call

Calls a function pointer obtained with `&funcname` syntax.

**Signature:** `(fn -- ...)`

```qd
fn double(x:i64 -- result:i64) {
	x 2 *
}

fn main() {
	&double -> fn_ptr
	5 fn_ptr call print nl  // 10
}
```

---

## Getting function pointers

Use `&` to get a pointer to a function:

```qd
&myfunction -> ptr
```

## Calling function pointers

Push arguments, then the pointer, then `call`:

```qd
// For: fn addition(a:i64 b:i64 -- sum:i64)
3 4 &addition call print nl  // 7
```

---

## Common uses

### Callbacks

```qd
fn for_each(arr:[]i64 callback:ptr -- ) {
	0 arr len 1 for i {
		arr i nth callback call
	}
}

fn print_item(x:i64 -- ) {
	x print " " print
}

fn main() {
	[1 2 3 4 5] &print_item for_each
	nl
}
```

### Function tables

```qd
fn op_add(a:i64 b:i64 -- r:i64) {
	a b +
}

fn op_sub(a:i64 b:i64 -- r:i64) {
	a b -
}

fn op_mul(a:i64 b:i64 -- r:i64) {
	a b *
}

fn main() {
	3 make<ptr> -> ops
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
fn map(arr:[]i64 f:ptr -- result:[]i64) {
	arr len make<i64> -> result
	0 arr len 1 for i {
		result i arr i nth f call set
	}
	result
}

fn square(x:i64 -- r:i64) { x x * }

fn main() {
	[1 2 3 4 5] &square map -> squared
	0 squared len 1 for i {
		squared i nth print " " print
	}
	nl  // 1 4 9 16 25
}
```

---

## See also

- [Function Pointers](../learn/7-advanced/function-pointers.md) in the Learn section
