# Function pointers

Function pointers let you store and call functions dynamically.

## Getting a function pointer

Use `&` to get a pointer to a function:

```qd
def double(x:i64 -- result:i64) {
	x 2 *
}

fn main() {
	&double -> fn_ptr
	// fn_ptr now holds a reference to double
}
```

## Calling function pointers

Use `call` to invoke a function pointer:

```qd
def double(x:i64 -- result:i64) {
	x 2 *
}

fn main() {
	&double -> fn_ptr
	5 fn_ptr call print nl  // 10
}
```

## Passing functions as arguments

```qd
def apply(x:i64 f:fn(i64 -- i64) -- result:i64) {
	x f call
}

def double(x:i64 -- result:i64) { x 2 * }
def square(x:i64 -- result:i64) { x x * }
def increment(x:i64 -- result:i64) { x 1 + }

fn main() {
	5 &double apply print nl  // 10
	5 &square apply print nl  // 25
	5 &increment apply print nl     // 6
}
```

## Storing in arrays

```qd
def addition(a:i64 b:i64 -- r:i64) {
	a b +
}

def subtraction(a:i64 b:i64 -- r:i64) {
	a b -
}

def multiplication(a:i64 b:i64 -- r:i64) {
	a b *
}

def div_op(a:i64 b:i64 -- r:i64) {
	a b /
}

fn main() {
	[4]ptr -> ops
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

Use function pointers for callbacks:

```qd
def for_each(arr:[]i64 callback:fn(i64 --) -- ) {
	0 arr len 1 for i {
		arr i nth callback call
	}
}

def print_item(x:i64 -- ) {
	x print " " print
}

fn main() {
	[1 2 3 4 5] &print_item for_each
	nl  // 1 2 3 4 5
}
```

## Higher-order functions

`map`, `filter` and the other combinators are builtins (see
[Higher-order functions](higher-order-functions.md)). Here is how such a function is written
by hand, as a function taking a function pointer.

### Map

```qd
def map_ints(arr:[]i64 f:fn(i64 -- i64) -- result:[]i64) {
	[arr len]i64 -> result
	0 arr len 1 for i {
		result i arr i nth f call set
	}
	result
}

def double(x:i64 -- r:i64) { x 2 * }

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
def filter_ints(arr:[]i64 pred:fn(i64 -- i64) -- result:[]i64) {
	// Count matches
	0 -> count
	0 arr len 1 for i {
		arr i nth pred call if {
			count 1 + -> count
		}
	}

	// Create result
	[count]i64 -> result
	0 -> j
	0 arr len 1 for i {
		arr i nth pred call if {
			result j arr i nth set
			j 1 + -> j
		}
	}
	result
}

def is_even(x:i64 -- result:i64) {
	x 2 % 0 ==
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
def reduce(arr:[]i64 initial:i64 f:fn(i64 i64 -- i64) -- result:i64) {
	initial -> result
	0 arr len 1 for i {
		result arr i nth f call -> result
	}
	result
}

def addition(a:i64 b:i64 -- r:i64) {
	a b +
}

fn main() {
	[1 2 3 4 5] 0 &addition reduce print nl  // 15
}
```

## Storing in structs

```qd
struct Handler {
	name:str
	func:ptr
}

def greet(name:str -- ) {
	"Hello, " print name print nl
}

def farewell(name:str -- ) {
	"Goodbye, " print name print nl
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
def handle_cmd(cmd:i64 -- ) {
	[4]ptr -> handlers
	handlers 0 &cmd_help set
	handlers 1 &cmd_list set
	handlers 2 &cmd_add set
	handlers 3 &cmd_quit set

	cmd 0 >= cmd 4 < and if {
		handlers cmd nth call
	} else {
		"Unknown command" print nl
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

Use `type` to create readable names for function pointer types:

<!-- doccheck: skip signature shown with its body elided -->
```qd
type Predicate = fn(i64 -- i64)
type Reducer = fn(i64 i64 -- i64)

def keep_if(arr:[]i64 pred:Predicate -- result:[]i64) { ... }
```

## What's next?

Learn about [Anonymous Functions](anonymous-functions.md) for inline function definitions.
