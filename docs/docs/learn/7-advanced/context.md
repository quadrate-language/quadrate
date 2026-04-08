# Context blocks (ctx)

Context blocks execute code in an isolated environment while preserving the parent stack.

## What is a context block?

A `ctx` block creates a copy of the current context (including the stack), executes statements in isolation, and optionally outputs a value to the parent.

```qd
ctx {
	statements
}
```

Stack effect: `(S -- S r)` where `S` is preserved and `r` is the optional output value (if one value is left on the child stack).

**Note**: Strings are deep-copied, but pointers (including struct pointers) are shallow-copied. Both parent and child will reference the same struct instances.

## Basic usage

```qd
fn main() {
	1 2 3
	ctx {
		// Child starts with copy of stack: [1, 2, 3]
		mul  // 2 * 3 = 6
		add  // 1 + 6 = 7
	}
	// Parent stack: [1, 2, 3, 7]
	// Original values preserved, result (7) added
	print nl  // 7
	print nl  // 3
	print nl  // 2
	print nl  // 1
}
```

Output:
```
7
3
2
1
```

## Stack isolation

The parent stack is completely isolated from modifications inside the `ctx` block:

```qd
fn main() {
	100 200 300
	ctx {
		clear      // Clears ONLY the child's stack
		42         // Push result
	}
	// Parent stack unchanged: [100, 200, 300, 42]
	print nl  // 42
	print nl  // 300
	print nl  // 200
	print nl  // 100
}
```

Output:
```
42
300
200
100
```

## Copy semantics

### Strings (deep copy)

Strings are deep-copied, so modifications in child don't affect parent:

```qd
fn main() {
	"original"
	ctx {
		drop
		"modified"
	}
	// Stack: ["original", "modified"]
	print nl  // modified
	print nl  // original
}
```

Output:
```
modified
original
```

### Pointers and structs (shallow copy)

Pointers are shallow-copied - parent and child share the same memory:

```qd
struct Counter {
	value:i64
}

fn main() {
	Counter { value = 0 } -> c

	c ctx {
		-> c2  // bind context value
		c2 c2 <<value 1 + >>value!  // Increment the struct's value in place
		c2 <<value  // Output the new value
	}
	print nl  // 1

	drop  // drop the c pointer pushed before ctx
	c <<value print nl  // 1 - parent sees the change!
}
```

Output:
```
1
1
```

If you need isolation for structs, create a new instance inside the `ctx` block.

## Chained context blocks

Context blocks can be chained for multi-step computations:

```qd
fn main() {
	ctx {
		10 20 add  // 30
	}
	ctx {
		2 mul      // 60
	}
	ctx {
		5 sub      // 55
	}
	// Stack: [30, 60, 55]
	print nl  // 55
	print nl  // 60
	print nl  // 30
}
```

Output:
```
55
60
30
```

## With control flow

Control flow works normally inside context blocks:

```qd
fn main() {
	10 5
	ctx {
		gt
		if {
			999
		} else {
			0
		}
	}
	// Stack: [10, 5, 999]
	print nl  // 999
	print nl  // 5
	print nl  // 10
}
```

## Use cases

### Safe computation

Compute a value without affecting the current stack:

```qd
fn compute_average(arr:[]i64 -- avg:f64) {
	-> arr  // bind parameter
	arr ctx {
		0.0 -> sum
		0 arr len 1 for i {
			arr i nth sum + -> sum
		}
		sum arr len /
	}
	// arr still on stack, result added
}
```

### Temporary workspace

Use the copied stack as temporary workspace:

```qd
fn process( -- result:i64) {
	1 2 3 4 5
	ctx {
		// Work with [1, 2, 3, 4, 5]
		add add add add  // Sum all: 15
		2 mul            // 30
	}
	// Original [1, 2, 3, 4, 5] preserved, 30 added
	// Can now decide what to keep
	-> result  // bind ctx result
	clear  // Clean up original values
	result
}
```

### Conditional results

Output different values based on conditions:

```qd
fn classify(x:i64 -- category:str) {
	-> x  // bind parameter
	x ctx {
		dup 0 < if {
			drop "negative"
		} else {
			0 == if {
				"zero"
			} else {
				"positive"
			}
		}
	}
}
```

## Key points

1. `ctx { }` creates an isolated copy of the entire context
2. The parent stack is completely preserved
3. One value can optionally be output from the block
4. Strings are deep-copied (isolated)
5. Pointers/structs are shallow-copied (shared)
6. Stack effect: `(S -- S)` or `(S -- S r)` if outputting a value

## What's next?

Learn about [Function Pointers](function-pointers.md) for dynamic dispatch.
