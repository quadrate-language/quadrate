# Structs

Structs group related data together into custom types.

## Defining structs

```qd
struct Point {
	x:f64
	y:f64
}

struct Person {
	name:str
	age:i64
}
```

## Creating instances

Use the struct name with field initializers:

```qd
struct Point {
	x:f64
	y:f64
}

fn main() {
	Point {
		x = 10.0
		y = 20.0
	} -> p
	p @x print nl  // 10
	p @y print nl  // 20
}
```

## Reading fields

Use `@fieldname` to read:

```qd
struct Rectangle {
	width:f64
	height:f64
}

fn area(rect:Rectangle -- a:f64) {
	-> rect  // bind parameter
	rect @width rect @height *
}

fn main() {
	Rectangle {
		width = 5.0
		height = 3.0
	} -> rect
	rect area print nl  // 15
}
```

## Writing fields

Use `.fieldname` to write:

```qd
struct Counter {
	value:i64
}

fn main() {
	Counter {
		value = 0
	} -> c

	c @value print nl  // 0

	10 c .value
	c @value print nl  // 10

	c @value inc c .value
	c @value print nl  // 11
}
```

## Nested structs

Structs can contain other structs:

```qd
struct Point {
	x:f64
	y:f64
}

struct Line {
	start:Point
	end:Point
}

fn main() {
	Point {
		x = 0.0
		y = 0.0
	} -> p1
	Point {
		x = 10.0
		y = 10.0
	} -> p2

	Line {
		start = p1
		end = p2
	} -> line

	line @start @x print nl  // 0
	line @end @x print nl    // 10
}
```

## Structs with arrays

```qd
struct Point {
	x:f64
	y:f64
}

struct Polygon {
	points:ptr
	count:i64
}

fn main() {
	3 make<ptr> -> pts
	Point {
		x = 0.0
		y = 0.0
	} -> p0
	Point {
		x = 1.0
		y = 0.0
	} -> p1
	Point {
		x = 0.5
		y = 1.0
	} -> p2
	pts 0 p0 set
	pts 1 p1 set
	pts 2 p2 set

	Polygon {
		points = pts
		count = 3
	} -> triangle

	triangle @count print nl  // 3
}
```

## Struct methods

Methods are functions that operate on a specific struct type. Define them using receiver syntax:

```qd
use math

struct Point {
	x:f64
	y:f64
}

// Method with receiver syntax: fn (receiver:Type) name(params -- returns)
fn (p:Point) magnitude( -- m:f64) {
	p @x dup * p @y dup * + math::sqrt
}

fn (p:Point) move(dx:f64 dy:f64 -- ) {
	-> dy -> dx  // bind parameters
	p @x dx + p .x
	p @y dy + p .y
}

fn main() {
	Point { x = 3.0 y = 4.0 } -> pt

	// Call method on struct - receiver is popped from stack
	pt magnitude print nl  // 5

	// Methods can also work on inline struct construction
	Point { x = 5.0 y = 12.0 } magnitude print nl  // 13

	// Method with parameters
	pt 1.0 1.0 move
	pt @x print nl  // 4
}
```

### Method resolution

When a struct is on the stack and you use an identifier, methods take precedence over global functions:

```qd
struct Counter {
	value:i64
}

fn (c:Counter) bump( -- result:i64) {
	c @value 1 +
}

fn bump( -- result:i64) {
	999
}

fn main() {
	Counter { value = 10 } -> c
	c bump print nl   // 11 (calls method)
	bump print nl     // 999 (calls global function)
}
```

### Traditional function style

You can also define functions that take structs as parameters:

```qd
use math

struct Point {
	x:f64
	y:f64
}

fn point_distance(p1:Point p2:Point -- d:f64) {
	-> p2 -> p1  // bind parameters
	p2 @x p1 @x - dup *
	p2 @y p1 @y - dup *
	+ math::sqrt
}

fn main() {
	Point { x = 0.0 y = 0.0 } -> a
	Point { x = 3.0 y = 4.0 } -> b

	a b point_distance print nl  // 5
}
```

## Common patterns

### Builder pattern

```qd
struct Config {
	debug:i64
	verbose:i64
	max_retries:i64
}

fn config_new( -- cfg:Config) {
	Config {
		debug = 0
		verbose = 0
		max_retries = 3
	}
}

fn config_set_debug(cfg:Config value:i64 -- cfg:Config) {
	-> value -> cfg
	value cfg .debug
	cfg
}

fn main() {
	config_new 1 config_set_debug -> cfg
	cfg @debug print nl  // 1
}
```

### Linked list node

```qd
struct Node {
	value:i64
	next:ptr
}

fn node_new(value:i64 -- node:ptr) {
	-> value  // bind parameter
	Node {
		value = value
		next = 0
	}
}

fn main() {
	10 node_new -> first
	20 node_new -> second
	30 node_new -> third

	second first .next
	third second .next

	// Traverse
	first -> current
	current 0 != while {
		current @value print nl
		current @next -> current
		current 0 !=
	}
	// Output: 10 20 30
}
```

### Stack data structure

```qd
struct Stack {
	data:ptr
	top:i64
	capacity:i64
}

fn stack_new(capacity:i64 -- s:Stack) {
	-> capacity  // bind parameter
	capacity make<i64> -> data
	Stack {
		data = data
		top = 0
		capacity = capacity
	}
}

fn stack_push(s:Stack value:i64 -- ) {
	-> value -> s
	s @data s @top value set
	s @top 1 + s .top
}

fn stack_pop(s:Stack -- value:i64) {
	-> s  // bind parameter
	s @top 1 - s .top
	s @data s @top nth
}

fn main() {
	10 stack_new -> s
	s 1 stack_push
	s 2 stack_push
	s 3 stack_push

	s stack_pop print nl  // 3
	s stack_pop print nl  // 2
	s stack_pop print nl  // 1
}
```

## Struct equality

Compare structs field by field:

```qd
struct Point {
	x:f64
	y:f64
}

fn points_equal(a:Point b:Point -- equal:i64) {
	-> b -> a
	a @x b @x == a @y b @y == and
}

fn main() {
	Point {
		x = 1.0
		y = 2.0
	} -> p1
	Point {
		x = 1.0
		y = 2.0
	} -> p2
	Point {
		x = 3.0
		y = 4.0
	} -> p3

	p1 p2 points_equal print nl  // 1 (true)
	p1 p3 points_equal print nl  // 0 (false)
}
```

## What's next?

Learn about [Constants](constants.md) to define fixed values.
