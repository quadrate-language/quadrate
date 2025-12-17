# Structs

Structs group related data together into custom types.

## Defining Structs

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

## Creating Instances

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
	p @x print nl  // 10.0
	p @y print nl  // 20.0
}
```

## Reading Fields

Use `@fieldname` to read:

```qd
struct Rectangle {
	width:f64
	height:f64
}

fn area(rect:ptr -- a:f64) {
	-> rect
	rect @width rect @height *
}

fn main() {
	Rectangle {
		width = 5.0
		height = 3.0
	} -> rect
	rect area print nl  // 15.0
}
```

## Writing Fields

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

## Nested Structs

Structs can contain other structs:

```qd
struct Point {
	x:f64
	y:f64
}

struct Line {
	start:ptr
	end:ptr
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

	line @start @x print nl  // 0.0
	line @end @x print nl    // 10.0
}
```

## Structs with Arrays

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

## Struct Methods

Define functions that operate on structs:

```qd
struct Point {
	x:f64
	y:f64
}

fn point_distance(p1:ptr p2:ptr -- d:f64) {
	-> p2 -> p1
	p2 @x p1 @x - dup *
	p2 @y p1 @y - dup *
	+ sqrt
}

fn point_move(p:ptr dx:f64 dy:f64 -- ) {
	-> dy -> dx -> p
	p @x dx + p .x
	p @y dy + p .y
}

fn main() {
	Point {
		x = 0.0
		y = 0.0
	} -> a
	Point {
		x = 3.0
		y = 4.0
	} -> b

	a b point_distance print nl  // 5.0

	a 1.0 1.0 point_move
	a @x print nl  // 1.0
}
```

## Common Patterns

### Builder Pattern

```qd
struct Config {
	debug:i64
	verbose:i64
	max_retries:i64
}

fn config_new( -- cfg:ptr) {
	Config {
		debug = 0
		verbose = 0
		max_retries = 3
	}
}

fn config_set_debug(cfg:ptr value:i64 -- cfg:ptr) {
	-> value -> cfg
	value cfg .debug
	cfg
}

fn main() {
	config_new 1 config_set_debug -> cfg
	cfg @debug print nl  // 1
}
```

### Linked List Node

```qd
struct Node {
	value:i64
	next:ptr
}

fn node_new(value:i64 -- node:ptr) {
	-> value
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

### Stack Data Structure

```qd
struct Stack {
	data:ptr
	top:i64
	capacity:i64
}

fn stack_new(capacity:i64 -- s:ptr) {
	-> capacity
	capacity make<i64> -> data
	Stack {
		data = data
		top = 0
		capacity = capacity
	}
}

fn stack_push(s:ptr value:i64 -- ) {
	-> value -> s
	s @data s @top value set
	s @top 1 + s .top
}

fn stack_pop(s:ptr -- value:i64) {
	-> s
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

## Struct Equality

Compare structs field by field:

```qd
struct Point {
	x:f64
	y:f64
}

fn points_equal(a:ptr b:ptr -- equal:i64) {
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

## What's Next?

Learn about [Constants](constants.md) to define fixed values.
