# Arrays

Arrays store multiple values of the same type.

## Creating Arrays

### Array Literals

```qd
fn main( -- ) {
	[1 2 3 4 5] -> arr
	arr @len print nl  // 5
}
```

### Empty Arrays with Size

```qd
fn main( -- ) {
	10 i64[] -> arr    // Array of 10 integers
	5 f64[] -> floats  // Array of 5 floats
	3 str[] -> strings // Array of 3 strings
}
```

## Accessing Elements

Use `@[]` to read elements (0-indexed):

```qd
fn main( -- ) {
	[10 20 30 40 50] -> arr

	arr 0 @[] print nl  // 10 (first)
	arr 2 @[] print nl  // 30 (third)
	arr 4 @[] print nl  // 50 (last)
}
```

## Setting Elements

Use `![]` to write elements:

```qd
fn main( -- ) {
	5 i64[] -> arr

	100 arr 0 ![]  // Set first element to 100
	200 arr 1 ![]  // Set second element to 200

	arr 0 @[] print nl  // 100
	arr 1 @[] print nl  // 200
}
```

## Array Length

Use `@len` to get the length:

```qd
fn main( -- ) {
	[1 2 3 4 5] -> arr
	arr @len print nl  // 5

	10 i64[] -> arr2
	arr2 @len print nl  // 10
}
```

## Iterating Arrays

### With for Loop

```qd
fn main( -- ) {
	[10 20 30 40 50] -> arr

	0 arr @len for i {
		arr i @[] print nl
	}
}
```

### With iter

```qd
fn main( -- ) {
	[10 20 30 40 50] -> arr

	arr iter for item {
		item print nl
	}
}
```

## Array Operations

### Sum

```qd
fn sum(arr:ptr -- total:i64) {
	-> arr
	0 -> total
	arr iter for item {
		total item + -> total
	}
	total
}

fn main( -- ) {
	[1 2 3 4 5] sum print nl  // 15
}
```

### Find Maximum

```qd
fn max(arr:ptr -- result:i64) {
	-> arr
	arr 0 @[] -> result
	arr iter for item {
		item result > if {
			item -> result
		}
	}
	result
}

fn main( -- ) {
	[3 1 4 1 5 9 2 6] max print nl  // 9
}
```

### Count Matches

```qd
fn count_if(arr:ptr value:i64 -- count:i64) {
	-> value -> arr
	0 -> count
	arr iter for item {
		item value == if {
			count 1 + -> count
		}
	}
	count
}

fn main( -- ) {
	[1 2 1 3 1 4 1] 1 count_if print nl  // 4
}
```

## Multi-dimensional Arrays

Create nested arrays:

```qd
fn main( -- ) {
	// 3x3 matrix
	3 ptr[] -> matrix

	[1 2 3] matrix 0 ![]
	[4 5 6] matrix 1 ![]
	[7 8 9] matrix 2 ![]

	// Access element at row 1, column 2
	matrix 1 @[] 2 @[] print nl  // 6
}
```

## Array of Floats

```qd
fn main( -- ) {
	[1.0 2.5 3.7 4.2] -> arr

	arr iter for x {
		x print nl
	}
}
```

## Array of Strings

```qd
fn main( -- ) {
	["apple" "banana" "cherry"] -> fruits

	fruits iter for fruit {
		fruit print nl
	}
}
```

## Copying Arrays

Arrays are references. To copy:

```qd
fn copy_array(src:ptr -- dst:ptr) {
	-> src
	src @len i64[] -> dst
	0 src @len for i {
		src i @[] dst i ![]
	}
	dst
}

fn main( -- ) {
	[1 2 3] -> original
	original copy_array -> copied

	// Modify copy doesn't affect original
	99 copied 0 ![]
	original 0 @[] print nl  // Still 1
	copied 0 @[] print nl    // 99
}
```

## Common Patterns

### Initialize with Value

```qd
fn fill(arr:ptr value:i64 -- ) {
	-> value -> arr
	0 arr @len for i {
		value arr i ![]
	}
}

fn main( -- ) {
	5 i64[] -> arr
	arr 42 fill
	arr iter for x { x print " " print }
	nl  // 42 42 42 42 42
}
```

### Reverse Array

```qd
fn reverse(arr:ptr -- ) {
	-> arr
	0 -> i
	arr @len 1 - -> j
	i j < while {
		arr i @[] -> temp
		arr j @[] arr i ![]
		temp arr j ![]
		i 1 + -> i
		j 1 - -> j
	}
}
```

### Filter Array

```qd
fn filter_positive(arr:ptr -- result:ptr) {
	-> arr
	// First count positives
	0 -> count
	arr iter for x {
		x 0 > if { count 1 + -> count }
	}
	// Create result array
	count i64[] -> result
	0 -> j
	arr iter for x {
		x 0 > if {
			x result j ![]
			j 1 + -> j
		}
	}
	result
}
```

## What's Next?

Learn about [Structs](structs.md) to create custom data types.
