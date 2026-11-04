# Arrays

Arrays store multiple values of the same type.

## Creating arrays

### Array literals

```qd
fn main() {
	[1 2 3 4 5] len print nl  // 5
}
```

### Sized arrays

Put the element type right after the closing bracket and the brackets hold a *size* instead
of elements. Every element starts at the zero value of that type.

```qd
fn main() {
	[10]i64 -> arr     // Array of 10 integers, each 0
	[5]f64 -> floats   // Array of 5 floats, each 0.0
	[3]str -> strings  // Array of 3 strings, each ""
	[]i64 -> empty     // Size left out, so none of them: an empty []i64
}
```

The type name has to touch the `]`. With a space it is an ordinary literal again, so `[10]i64`
is ten zeros while `[10]` is a one-element array holding the number 10.

The size can be any expression that leaves one integer:

```qd
fn main() {
	[1 2 3] -> src
	[src len]i64 -> dst    // As long as src
	dst len print nl       // 3
}
```

## Accessing elements

Use `nth` to read elements (0-indexed):

```qd
fn main() {
	[10 20 30 40 50]
	dup 0 nth print nl  // 10 (first)
	dup 2 nth print nl  // 30 (third)
	4 nth print nl      // 50 (last)
}
```

## Setting elements

Use `set` to write elements:

```qd
fn main() {
	[5]i64
	dup 0 100 set  // Set first element to 100
	dup 1 200 set  // Set second element to 200

	dup 0 nth print nl  // 100
	1 nth print nl      // 200
}
```

## Array length

Use `len` to get the length:

```qd
fn main() {
	[1 2 3 4 5] len print nl  // 5

	[10]i64 len print nl      // 10
}
```

## Appending elements

Use `append` to add elements to an array:

```qd
fn main() {
	[]i64                 // Start with an empty []i64
	10 append             // Append 10
	20 append             // Append 20
	30 append             // Append 30

	dup len print nl      // 3
	dup 0 nth print nl    // 10
	2 nth print nl        // 30
}
```

`append` returns the modified array, so you can chain appends or reassign with `->`.

## Iterating arrays

### With for loop

```qd
fn main() {
	[10 20 30 40 50]
	0 over len 1 for i {
		dup i nth print nl
	}
	drop
}
```

## Typed array parameters

Use `[]T` to declare the element type in function signatures:

<!-- doccheck: skip signatures shown with their bodies elided -->
```qd
fn sum(arr:[]i64 -- total:i64) { ... }
fn names(arr:[]str -- ) { ... }
fn coords(arr:[]f64 -- ) { ... }
```

The compiler checks that you pass the right array type and that `nth` returns the correct element type.

## Array operations

### Sum

```qd
fn sum(arr:[]i64 -- total:i64) {
	-> arr
	0
	0 arr len 1 for i {
		arr i nth +
	}
}

fn main() {
	[1 2 3 4 5] sum print nl  // 15
}
```

### Find maximum

```qd
fn larger(a:i64 b:i64 -- c:i64) {
	dup2 < if { nip } else { drop }
}

fn max(arr:[]i64 -- result:i64) {
	-> arr
	arr 0 nth
	0 arr len 1 for i {
		arr i nth larger
	}
}

fn main() {
	[3 1 4 1 5 9 2 6] max print nl  // 9
}
```

### Count matches

```qd
fn count_if(arr:[]i64 value:i64 -- count:i64) {
	-> value -> arr
	0
	0 arr len 1 for i {
		arr i nth value == if {
			1 +
		}
	}
}

fn main() {
	[1 2 1 3 1 4 1] 1 count_if print nl  // 4
}
```

## Array of floats

```qd
fn main() {
	[1.0 2.5 3.7 4.2]
	0 over len 1 for i {
		dup i nth print nl
	}
	drop
}
```

## Array of strings

```qd
fn main() {
	["apple" "banana" "cherry"]
	0 over len 1 for i {
		dup i nth print nl
	}
	drop
}
```

## Copying arrays

Arrays are references. To copy:

```qd
fn copy_array(src:[]i64 -- dst:[]i64) {
	-> src
	[src len]i64
	0 src len 1 for i {
		dup i src i nth set
	}
}

fn main() {
	[1 2 3] -> original
	original copy_array -> copied

	// Modify copy doesn't affect original
	copied 0 99 set
	original 0 nth print nl  // Still 1
	copied 0 nth print nl    // 99
}
```

## Common patterns

### Initialize with value

```qd
fn fill(arr:[]i64 value:i64 -- ) {
	-> value -> arr
	0 arr len 1 for i {
		arr i value set
	}
}

fn main() {
	[5]i64
	dup 42 fill
	0 over len 1 for i {
		dup i nth print " " print
	}
	drop
	nl  // 42 42 42 42 42
}
```

### Reverse array

```qd
fn reverse(arr:[]i64 -- ) {
	-> arr
	0 -> i
	arr len 1 - -> j
	loop {
		i j >= if { break }
		arr i nth -> temp
		arr i arr j nth set
		arr j temp set
		i 1 + -> i
		j 1 - -> j
	}
}

fn main() {
	[1 2 3 4 5]
	dup reverse
	0 over len 1 for i {
		dup i nth print " " print
	}
	drop
	nl
}
```

### Filter array

```qd
fn filter_positive(arr:[]i64 -- result:[]i64) {
	-> arr
	[]i64
	0 arr len 1 for i {
		arr i nth 0 > if {
			arr i nth append
		}
	}
}

fn main() {
	[1 2 -3 -4 5] filter_positive
	0 over len 1 for i {
		dup i nth print " " print
	}
	drop
	nl
}
```

## What's next?

Learn about [Structs](structs.md) to create custom data types.
