# Function values

A function is a value like any other: it can be stored in a local, passed to another function,
kept in a struct field or an array, and called later. Its type is written `fn(...)` with the
signature inside, such as `fn(i64 -- i64)`.

## Getting a function value

Use `&` in front of a function's name to get it as a value:

```qd
fn double(x:i64 -- result:i64) {
	2 *
}

fn main() {
	&double -> f
	// f now holds double as a value of type fn(i64 -- i64)
}
```

A function value is a counted value, like a struct or an array. `&name` gives a static one: it
refers to the named function and carries nothing else. An
[anonymous function](anonymous-functions.md) that captures locals carries them with it.

## Calling function values

Use `call` to invoke a function value:

```qd
fn double(x:i64 -- result:i64) {
	2 *
}

fn main() {
	5 &double call print nl  // 10
}
```

## Passing functions as arguments

```qd
fn apply(x:i64 f:fn(i64 -- i64) -- result:i64) {
	call
}

fn double(x:i64 -- result:i64) { 2 * }
fn square(x:i64 -- result:i64) { dup * }
fn increment(x:i64 -- result:i64) { 1 + }

fn main() {
	5 &double apply print nl  // 10
	5 &square apply print nl  // 25
	5 &increment apply print nl     // 6
}
```

## Storing in arrays

`[4]fn(i64 i64 -- i64)` makes a function table: an array of four function values with that
signature.

```qd
fn addition(a:i64 b:i64 -- r:i64) {
	+
}

fn subtraction(a:i64 b:i64 -- r:i64) {
	-
}

fn multiplication(a:i64 b:i64 -- r:i64) {
	*
}

fn div_op(a:i64 b:i64 -- r:i64) {
	/
}

fn main() {
	[4]fn(i64 i64 -- i64) -> ops
	ops 0 &addition set
	ops 1 &subtraction set
	ops 2 &multiplication set
	ops 3 &div_op set

	10 5 ops 0 nth call print nl  // 15 (add)
	10 5 ops 1 nth call print nl  // 5 (sub)
	10 5 ops 2 nth call print nl  // 50 (mul)
	10 5 ops 3 nth call print nl  // 2 (div)
}
```

## Callbacks

Use function values for callbacks:

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
	nl  // 1 2 3 4 5
}
```

## Higher-order functions

`map`, `filter` and the other combinators are builtins (see
[Higher-order functions](higher-order-functions.md)). Here is how such a function is written
by hand, as a function taking a function value.

### Map

```qd
fn map_ints(arr:[]i64 f:fn(i64 -- i64) -- result:[]i64) {
	-> f -> arr
	[]i64
	0 arr len 1 for i {
		arr i nth f call append
	}
}

fn double(x:i64 -- r:i64) { 2 * }

fn main() {
	[1 2 3 4 5] &double map_ints -> doubled
	0 doubled len 1 for i {
		doubled i nth print " " print
	}
	nl  // 2 4 6 8 10
}
```

### Filter

```qd
fn filter_ints(arr:[]i64 pred:fn(i64 -- i64) -- result:[]i64) {
	-> pred -> arr
	[]i64
	0 arr len 1 for i {
		arr i nth pred call if {
			arr i nth append
		}
	}
}

fn is_even(x:i64 -- result:i64) {
	2 % 0 ==
}

fn main() {
	[1 2 3 4 5 6 7 8 9 10] &is_even filter_ints -> evens
	0 evens len 1 for i {
		evens i nth print " " print
	}
	nl  // 2 4 6 8 10
}
```

### Reduce

```qd
fn reduce(arr:[]i64 initial:i64 f:fn(i64 i64 -- i64) -- result:i64) {
	-> f -> initial -> arr
	initial
	0 arr len 1 for i {
		arr i nth f call
	}
}

fn addition(a:i64 b:i64 -- r:i64) {
	+
}

fn main() {
	[1 2 3 4 5] 0 &addition reduce print nl  // 15
}
```

## Storing in structs

```qd
struct Handler {
	name:str
	func:fn(str --)
}

fn greet(name:str -- ) {
	"Hello, " print print nl
}

fn farewell(name:str -- ) {
	"Goodbye, " print print nl
}

fn main() {
	Handler {
		name = "greeter"
		func = &greet
	} -> h1
	Handler {
		name = "fareweller"
		func = &farewell
	} -> h2

	"Alice" h1 <<func call
	"Bob" h2 <<func call
}
```

## Function tables

Dispatch based on a selector:

```qd
fn handle_cmd(cmd:i64 -- ) {
	dup 0 3 within if {
		[&cmd_help &cmd_list &cmd_add &cmd_quit] swap nth call
	} else {
		drop "Unknown command" print nl
	}
}

fn cmd_help() {
	"Help message" print nl
}

fn cmd_list() {
	"Listing items" print nl
}

fn cmd_add() {
	"Adding item" print nl
}

fn cmd_quit() {
	"Quitting" print nl
}
```

## Type aliases

Use `type` to create readable names for function types:

<!-- doccheck: skip signature shown with its body elided -->
```qd
type Predicate = fn(i64 -- i64)
type Reducer = fn(i64 i64 -- i64)

fn keep_if(arr:[]i64 pred:Predicate -- result:[]i64) { ... }
```

## What's next?

Learn about [Anonymous Functions](anonymous-functions.md) for inline function definitions.
