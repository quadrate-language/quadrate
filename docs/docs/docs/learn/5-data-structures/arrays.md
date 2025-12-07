# Arrays

Arrays store multiple values of the same type.

## Creating Arrays

### Array Literals

```qd
fn main( -- ) {
	[1 2 3 4 5] -> arr
	arr len print nl  // 5
}
```

### Empty Arrays with Size

```qd
fn main( -- ) {
	10 make<i64> -> arr     // Array of 10 integers
	5 make<f64> -> floats   // Array of 5 floats
	3 make<str> -> strings  // Array of 3 strings
}
```

## Accessing Elements

Use `nth` to read elements (0-indexed):

```qd
fn main( -- ) {
	[10 20 30 40 50] -> arr

	arr 0 nth print nl  // 10 (first)
	arr 2 nth print nl  // 30 (third)
	arr 4 nth print nl  // 50 (last)
}
```

## Setting Elements

Use `set` to write elements:

```qd
fn main( -- ) {
	5 make<i64> -> arr

	arr 0 100 set  // Set first element to 100
	arr 1 200 set  // Set second element to 200

	arr 0 nth print nl  // 100
	arr 1 nth print nl  // 200
}
```

## Array Length

Use `len` to get the length:

```qd
fn main( -- ) {
	[1 2 3 4 5] -> arr
	arr len print nl  // 5

	10 make<i64> -> arr2
	arr2 len print nl  // 10
}
```

## Iterating Arrays

### With for Loop

```qd
fn main( -- ) {
	[10 20 30 40 50] -> arr

	0 arr len 1 for i {
		arr i nth print nl
	}
}
```

## Array Operations

### Sum

```qd
fn sum(arr:ptr -- total:i64) {
	-> arr
	0 -> total
	0 arr len 1 for i {
		total arr i nth + -> total
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
	arr 0 nth -> result
	0 arr len 1 for i {
		arr i nth result > if {
			arr i nth -> result
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
	0 arr len 1 for i {
		arr i nth value == if {
			count 1 + -> count
		}
	}
	count
}

fn main( -- ) {
	[1 2 1 3 1 4 1] 1 count_if print nl  // 4
}
```

## Array of Floats

```qd
fn main( -- ) {
	[1.0 2.5 3.7 4.2] -> arr

	0 arr len 1 for i {
		arr i nth print nl
	}
}
```

## Array of Strings

```qd
fn main( -- ) {
	["apple" "banana" "cherry"] -> fruits

	0 fruits len 1 for i {
		fruits i nth print nl
	}
}
```

## Copying Arrays

Arrays are references. To copy:

```qd
fn copy_array(src:ptr -- dst:ptr) {
	-> src
	src len make<i64> -> dst
	0 src len 1 for i {
		dst i src i nth set
	}
	dst
}

fn main( -- ) {
	[1 2 3] -> original
	original copy_array -> copied

	// Modify copy doesn't affect original
	copied 0 99 set
	original 0 nth print nl  // Still 1
	copied 0 nth print nl    // 99
}
```

## Common Patterns

### Initialize with Value

```qd
fn fill(arr:ptr value:i64 -- ) {
	-> value -> arr
	0 arr len 1 for i {
		arr i value set
	}
}

fn main( -- ) {
	5 make<i64> -> arr
	arr 42 fill
	0 arr len 1 for i {
		arr i nth print " " print
	}
	nl  // 42 42 42 42 42
}
```

### Reverse Array

```qd
fn reverse(arr:ptr -- ) {
	-> arr
	0 -> i
	arr len 1 - -> j
	i j < while {
		arr i nth -> temp
		arr i arr j nth set
		arr j temp set
		i 1 + -> i
		j 1 - -> j
		i j <
	}
}

fn main( -- ) {
	[1 2 3 4 5] -> arr
	arr reverse
	0 arr len 1 for i {
		arr i nth print " " print
	}
	nl
}
```

### Filter Array

```qd
fn filter_positive(arr:ptr -- result:ptr) {
	-> arr
	// First count positives
	0 -> count
	0 arr len 1 for i {
		arr i nth 0 > if {
			count inc -> count
		}
	}
	// Create result array
	count make<i64> -> result
	0 -> j
	0 arr len 1 for i {
		arr i nth 0 > if {
			result j arr i nth set
			j inc -> j
		}
	}
	result
}

fn main( -- ) {
	[1 2 -3 -4 5] -> arr
	arr filter_positive -> result
	0 result len 1 for i {
		result i nth print " " print
	}
	print nl
}
```

## What's Next?

Learn about [Structs](structs.md) to create custom data types.
