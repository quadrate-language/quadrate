# Loops

Quadrate provides several ways to repeat operations.

## while Loops

Repeat while a condition is true:

```qd
fn main( -- ) {
	0 -> i
	i 5 < while {
		i print nl
		i 1 + -> i
	}
	// Output: 0 1 2 3 4
}
```

The condition is checked before each iteration.

## for Loops

Iterate over a range:

```qd
fn main( -- ) {
	0 5 for i {
		i print nl
	}
	// Output: 0 1 2 3 4
}
```

The range is `[start, end)` - includes start, excludes end.

## for with Step

Specify a custom step:

```qd
fn main( -- ) {
	0 10 2 for i {
		i print nl
	}
	// Output: 0 2 4 6 8
}
```

## Counting Down

Use negative step:

```qd
fn main( -- ) {
	5 0 -1 for i {
		i print nl
	}
	// Output: 5 4 3 2 1
}
```

## Iterating Over Arrays

Use `iter` for array iteration:

```qd
fn main( -- ) {
	[1 2 3 4 5] -> arr

	arr iter for item {
		item print " " print
	}
	nl
	// Output: 1 2 3 4 5
}
```

## loop - Infinite Loop

Use `loop` with `break`:

```qd
fn main( -- ) {
	0 -> count
	loop {
		count print nl
		count 1 + -> count
		count 5 >= if {
			break
		}
	}
	// Output: 0 1 2 3 4
}
```

## break - Exit Loop

Exit immediately:

```qd
fn main( -- ) {
	0 10 for i {
		i 5 == if {
			break
		}
		i print nl
	}
	// Output: 0 1 2 3 4
}
```

## continue - Skip Iteration

Skip to next iteration:

```qd
fn main( -- ) {
	0 10 for i {
		i 2 % 0 == if {
			continue
		}
		i print nl
	}
	// Output: 1 3 5 7 9 (odd numbers only)
}
```

## Nested Loops

Loops can be nested:

```qd
fn main( -- ) {
	1 4 for i {
		1 4 for j {
			i print " * " print j print " = " print
			i j * print nl
		}
	}
}
```

## Loop with Accumulator

Build up a result:

```qd
fn sum_to_n(n:i64 -- sum:i64) {
	-> n
	0 -> sum
	1 n 1 + for i {
		sum i + -> sum
	}
	sum
}

fn main( -- ) {
	10 sum_to_n print nl  // 55
}
```

## Finding Elements

Search with early exit:

```qd
fn contains(arr:ptr value:i64 -- found:i64) {
	-> value -> arr
	0 -> found

	arr iter for item {
		item value == if {
			1 -> found
			break
		}
	}
	found
}

fn main( -- ) {
	[1 2 3 4 5] 3 contains print nl  // 1
	[1 2 3 4 5] 9 contains print nl  // 0
}
```

## Common Patterns

### Sum Array

```qd
fn sum_array(arr:ptr -- total:i64) {
	-> arr
	0 -> total
	arr iter for item {
		total item + -> total
	}
	total
}
```

### Count Matches

```qd
fn count_positive(arr:ptr -- count:i64) {
	-> arr
	0 -> count
	arr iter for item {
		item 0 > if {
			count 1 + -> count
		}
	}
	count
}
```

### Transform Array

```qd
fn double_all(arr:ptr -- result:ptr) {
	-> arr
	arr @len i64[] -> result
	0 arr @len for i {
		arr i @[] 2 * result i ![]
	}
	result
}
```

## What's Next?

Learn about [Switch Statements](switch.md) for multi-way branching.
