# Loops

Quadrate provides several ways to repeat operations.

## for loops

Iterate over a range:

```qd
fn main() {
	0 5 1 for i {
		i print nl
	}
	// Output: 0 1 2 3 4
}
```

The syntax is `start end step for <iterator> { body }`:

- **start** - Initial value (inclusive)
- **end** - Final value (exclusive)
- **step** - Increment per iteration
- **iterator** - A named variable (e.g., `i`) that holds the current value and is accessible inside the loop body

The range is `[start, end)` - includes start, excludes end.

## for with step

Specify a custom step:

```qd
fn main() {
	0 10 2 for i {
		i print nl
	}
	// Output: 0 2 4 6 8
}
```

## Counting down

Use negative step:

```qd
fn main() {
	5 0 -1 for i {
		i print nl
	}
	// Output: 5 4 3 2 1
}
```

## Iterating over arrays

Use index-based iteration:

```qd
fn main() {
	[1 2 3 4 5] -> arr

	0 arr len 1 for i {
		arr i nth print " " print
	}
	nl
	// Output: 1 2 3 4 5
}
```

## loop - infinite loop

Use `loop` with `break`:

```qd
fn main() {
	0 -> count
	loop {
		count print nl
		count inc -> count
		count 5 >= if {
			break
		}
	}
	// Output: 0 1 2 3 4
}
```

## while and until - conditional exits

Inside a `loop`, `while` takes a flag and leaves the loop when it is false:

```qd
fn main() {
	0 -> count
	loop {
		count 5 < while
		count print nl
		count 1 + -> count
	}
	// Output: 0 1 2 3 4
}
```

`until` is the same with the flag the other way round: it leaves when the flag is true. Pick
whichever lets you write the condition without `not`.

Where you put the word decides the shape of the loop. At the top the body may run no times; at
the bottom it runs at least once:

```qd
fn main() {
	0 -> tries
	loop {
		tries 1 + -> tries
		tries 3 >= until
	}
	tries print nl
	// Output: 3
}
```

In the middle, the loop leaves between the two halves of an iteration:

```qd
fn main() {
	0 -> n
	loop {
		n 1 + -> n
		n 4 == until    // the line below does not run on the last pass
		n print nl
	}
	// Output: 1 2 3
}
```

Both are just a conditional `break`, so they also work in a `for`, and they leave the innermost
loop. Since the condition is simply what comes before the word, a value waiting on the stack
under the loop is left alone:

```qd
fn main() {
	0 -> i
	42                     // still there after the loop
	loop {
		i 3 < while
		i 1 + -> i
	}
	print nl
	// Output: 42
}
```

## break - exit loop

Exit immediately:

```qd
fn main() {
	0 10 1 for i {
		i 5 == if {
			break
		}
		i print nl
	}
	// Output: 0 1 2 3 4
}
```

## continue - skip iteration

Skip to next iteration:

```qd
fn main() {
	0 10 1 for i {
		i 2 % 0 == if {
			continue
		}
		i print nl
	}
	// Output: 1 3 5 7 9 (odd numbers only)
}
```

## Nested loops

Loops can be nested:

```qd
fn main() {
	1 4 1 for i {
		1 4 1 for j {
			i print " * " print j print " = " print
			i j * print nl
		}
	}
}
```

## Loop with accumulator

Build up a result:

```qd
fn sum_to_n(n:i64 -- sum:i64) {
	0 -> sum
	1 n 1 + 1 for i {
		sum i + -> sum
	}
	sum
}

fn main() {
	10 sum_to_n print nl  // 55
}
```

## Finding elements

Search with early exit:

```qd
fn contains(arr:[]i64 value:i64 -- found:i64) {
	0 -> found

	0 arr len 1 for i {
		arr i nth value == if {
			1 -> found
			break
		}
	}
	found
}

fn main() {
	[1 2 3 4 5] 3 contains print nl  // 1
	[1 2 3 4 5] 9 contains print nl  // 0
}
```

## Common patterns

### Sum array

```qd
fn sum_array(arr:[]i64 -- total:i64) {
	0 -> total
	0 arr len 1 for i {
		total arr i nth cast<i64> + -> total
	}
	total
}
```

### Count matches

```qd
fn count_positive(arr:[]i64 -- count:i64) {
	0 -> count
	0 arr len 1 for i {
		arr i nth 0 > if {
			count 1 + -> count
		}
	}
	count
}
```

## What's next?

Learn about [Switch Statements](switch.md) for multi-way branching.
