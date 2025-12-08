# Miscellaneous

Other built-in operations.

## Overview

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| `call` | `( fn -- ... )` | Call a function pointer |

---

## Function Pointers

### call

Calls a function pointer obtained with `&funcname` syntax.

**Signature:** `( fn -- ... )`

```qd
fn double(x:i64 -- result:i64) {
	2 *
}

fn main( -- ) {
	&double -> fn_ptr
	5 fn_ptr call print nl  // 10
}
```

---

## Getting Function Pointers

Use `&` to get a pointer to a function:

```qd
&myfunction -> ptr
```

## Calling Function Pointers

Push arguments, then the pointer, then `call`:

```qd
// For: fn addition(a:i64 b:i64 -- sum:i64)
3 4 &addition call print nl  // 7
```

---

## Common Uses

### Callbacks

```qd
fn for_each(arr:ptr callback:ptr -- ) {
	-> callback -> arr
	0 arr len 1 for i {
		arr i nth callback call
	}
}

fn print_item(x:i64 -- ) {
	-> x
	x print " " print
}

fn main( -- ) {
	[1 2 3 4 5] &print_item for_each
	nl
}
```

### Function Tables

```qd
use mem

fn op_add(a:i64 b:i64 -- r:i64) {
	+
}

fn op_sub(a:i64 b:i64 -- r:i64) {
	-
}

fn op_mul(a:i64 b:i64 -- r:i64) {
	*
}

fn main( -- ) {
	3 make<ptr> -> ops
	&op_add ops 0 mem::set_ptr
	&op_sub ops 1 mem::set_ptr
	&op_mul ops 2 mem::set_ptr

	10 5 ops 0 nth call print nl  // 15
	10 5 ops 1 nth call print nl  // 5
	10 5 ops 2 nth call print nl  // 50
}
```

### Higher-Order Functions

```qd
fn map(arr:ptr f:ptr -- result:ptr) {
	-> f -> arr
	arr len make<i64> -> result
	0 arr len for i {
		arr i nth f call result i set
	}
	result
}

fn square(x:i64 -- r:i64) { dup * }

fn main( -- ) {
	[1 2 3 4 5] &square map -> squared
	// squared = [1, 4, 9, 16, 25]
}
```

---

## See Also

- [Function Pointers](../learn/7-advanced/function-pointers.md) in the Learn section
