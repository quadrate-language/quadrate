# Function Pointers

Function pointers let you store and call functions dynamically.

## Getting a Function Pointer

Use `&` to get a pointer to a function:

```qd
fn double(x:i64 -- result:i64) {
	2 *
}

fn main( -- ) {
	&double -> fn_ptr
	// fn_ptr now holds a reference to double
}
```

## Calling Function Pointers

Use `call` to invoke a function pointer:

```qd
fn double(x:i64 -- result:i64) {
	2 *
}

fn main( -- ) {
	&double -> fn_ptr
	5 fn_ptr call print nl  // 10
}
```

## Passing Functions as Arguments

```qd
fn apply(x:i64 f:ptr -- result:i64) {
	-> f -> x
	x f call
}

fn double(x:i64 -- result:i64) { 2 * }
fn square(x:i64 -- result:i64) { dup * }
fn inc(x:i64 -- result:i64) { 1 + }

fn main( -- ) {
	5 &double apply print nl  // 10
	5 &square apply print nl  // 25
	5 &inc apply print nl     // 6
}
```

## Storing in Arrays

```qd
fn add(a:i64 b:i64 -- r:i64) { + }
fn sub(a:i64 b:i64 -- r:i64) { - }
fn mul(a:i64 b:i64 -- r:i64) { * }
fn div_op(a:i64 b:i64 -- r:i64) { / }

fn main( -- ) {
	4 ptr[] -> ops
	&add ops 0 ![]
	&sub ops 1 ![]
	&mul ops 2 ![]
	&div_op ops 3 ![]

	10 5 ops 0 @[] call print nl  // 15 (add)
	10 5 ops 1 @[] call print nl  // 5 (sub)
	10 5 ops 2 @[] call print nl  // 50 (mul)
	10 5 ops 3 @[] call print nl  // 2 (div)
}
```

## Callbacks

Use function pointers for callbacks:

```qd
fn for_each(arr:ptr callback:ptr -- ) {
	-> callback -> arr
	arr iter for item {
		item callback call
	}
}

fn print_item(x:i64 -- ) {
	-> x
	x print " " print
}

fn main( -- ) {
	[1 2 3 4 5] &print_item for_each
	nl  // 1 2 3 4 5
}
```

## Higher-Order Functions

### Map

```qd
fn map(arr:ptr f:ptr -- result:ptr) {
	-> f -> arr
	arr @len i64[] -> result
	0 arr @len for i {
		arr i @[] f call result i ![]
	}
	result
}

fn double(x:i64 -- r:i64) { 2 * }

fn main( -- ) {
	[1 2 3 4 5] &double map -> doubled
	doubled iter for x {
		x print " " print
	}
	nl  // 2 4 6 8 10
}
```

### Filter

```qd
fn filter(arr:ptr pred:ptr -- result:ptr) {
	-> pred -> arr

	// Count matches
	0 -> count
	arr iter for x {
		x pred call if {
			count 1 + -> count
		}
	}

	// Create result
	count i64[] -> result
	0 -> j
	arr iter for x {
		x pred call if {
			x result j ![]
			j 1 + -> j
		}
	}
	result
}

fn is_even(x:i64 -- result:i64) {
	-> x
	x 2 % 0 ==
}

fn main( -- ) {
	[1 2 3 4 5 6 7 8 9 10] &is_even filter -> evens
	evens iter for x {
		x print " " print
	}
	nl  // 2 4 6 8 10
}
```

### Reduce

```qd
fn reduce(arr:ptr initial:i64 f:ptr -- result:i64) {
	-> f -> result -> arr
	arr iter for x {
		result x f call -> result
	}
	result
}

fn add(a:i64 b:i64 -- r:i64) { + }

fn main( -- ) {
	[1 2 3 4 5] 0 &add reduce print nl  // 15
}
```

## Storing in Structs

```qd
struct Handler {
	name:str
	fn:ptr
}

fn greet(name:str -- ) {
	-> name
	"Hello, " print name print nl
}

fn farewell(name:str -- ) {
	-> name
	"Goodbye, " print name print nl
}

fn main( -- ) {
	Handler { name = "greeter" fn = &greet } -> h1
	Handler { name = "fareweller" fn = &farewell } -> h2

	"Alice" h1 @fn call
	"Bob" h2 @fn call
}
```

## Function Tables

Dispatch based on a selector:

```qd
fn handle_cmd(cmd:i64 -- ) {
	-> cmd

	4 ptr[] -> handlers
	&cmd_help handlers 0 ![]
	&cmd_list handlers 1 ![]
	&cmd_add handlers 2 ![]
	&cmd_quit handlers 3 ![]

	cmd 0 >= cmd 4 < and if {
		handlers cmd @[] call
	} else {
		"Unknown command" print nl
	}
}

fn cmd_help( -- ) { "Help message" print nl }
fn cmd_list( -- ) { "Listing items" print nl }
fn cmd_add( -- ) { "Adding item" print nl }
fn cmd_quit( -- ) { "Quitting" print nl }
```

## What's Next?

Learn about [Memory Management](memory.md) for manual memory control.
