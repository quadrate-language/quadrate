# Tutorial: Structs

Structs let you group related data together. This tutorial covers defining structs, creating instances, accessing fields, and memory management.

## Defining a Struct

Use `struct` to define a new type:

```qd
struct Point {
	x:f64
	y:f64
}
```

Field syntax is `name:type`. Supported types:

- `i64` - integer
- `f64` - float
- `str` - string
- `ptr` - pointer (for nested structs)

## Creating Instances

Use the struct name followed by field assignments in braces:

```qd
fn main() {
	Point {
		x = 3.0
		y = 4.0
	} -> p

	// Or on one line
	Point { x = 1.0 y = 2.0 } -> p2
}
```

## Accessing Fields

Use `@fieldname` to access a field:

```qd
fn main() {
	Point { x = 3.0 y = 4.0 } -> p

	p @x print nl  // Prints: 3
	p @y print nl  // Prints: 4
}
```

Field access works on any struct pointer on the stack:

```qd
fn make_point(x:f64 y:f64 -- p:ptr) {
	-> y -> x
	Point { x = x y = y }
}

fn main() {
	// Access field directly from function return
	3.0 4.0 make_point @x print nl  // Prints: 3
}
```

## Modifying Fields

Use `.fieldname` to set a field value:

```qd
fn main() {
	Point { x = 0.0 y = 0.0 } -> p

	// Modify x field
	5.0 p .x

	// Modify y field
	10.0 p .y

	p @x print nl  // Prints: 5
	p @y print nl  // Prints: 10
}
```

The syntax is: `value struct .fieldname`

## Memory Management

Structs are allocated on the heap with reference counting.

### Automatic Cleanup

Structs are automatically freed when they go out of scope:

```qd
fn create_point() {
	Point { x = 1.0 y = 2.0 } -> p
	p @x print nl
	// p is automatically freed when function returns
}

fn main() {
	create_point
}
```

### Manual Cleanup

You can also free structs explicitly:

```qd
fn main() {
	Point { x = 1.0 y = 2.0 } -> p
	p @x print nl
	p free  // Explicitly free
}
```

## Constants with Structs

Use `const` for configuration values:

```qd
const OriginX = 0.0
const OriginY = 0.0

struct Point {
	x:f64
	y:f64
}

fn new_origin( -- p:ptr) {
	Point { x = OriginX y = OriginY }
}

fn main() {
	new_origin -> p
	p @x print nl  // Prints: 0
	p @y print nl  // Prints: 0
}
```

## Structs with Functions

Pass structs to functions using `ptr` type:

```qd
use math

struct Point {
	x:f64
	y:f64
}

fn distance_from_origin(p:ptr -- dist:f64) {
	-> p
	p @x dup *     // x squared
	p @y dup * +   // + y squared
	math::sqrt     // square root
}

fn print_point(p:ptr -- ) {
	-> p
	"(" print p @x print ", " print p @y print ")" print nl
}

fn main() {
	Point { x = 3.0 y = 4.0 } -> p

	p print_point              // Prints: (3, 4)
	p distance_from_origin print nl  // Prints: 5
}
```

## Arrays of Structs

Create arrays of struct pointers:

```qd
fn main() {
	// Create array to hold 3 Point pointers
	3 make<Point> -> points

	// Create and store points
	Point { x = 1.0 y = 2.0 } -> p1
	Point { x = 3.0 y = 4.0 } -> p2
	Point { x = 5.0 y = 6.0 } -> p3

	points 0 p1 set
	points 1 p2 set
	points 2 p3 set

	// Access points from array
	points 0 nth @x print nl  // Prints: 1
	points 1 nth @y print nl  // Prints: 4
	points 2 nth @x print nl  // Prints: 5

	// Clean up
	points free
}
```

## Nested Structs

Use `ptr` fields for nested structures:

```qd
struct Point {
	x:f64
	y:f64
}

struct Rectangle {
	top_left:ptr
	bottom_right:ptr
}

fn main() {
	Point { x = 0.0 y = 0.0 } -> tl
	Point { x = 10.0 y = 5.0 } -> br

	Rectangle {
		top_left = tl
		bottom_right = br
	} -> rect

	rect @top_left @x print nl      // Prints: 0
	rect @bottom_right @y print nl  // Prints: 5
}
```

## Complete Example: Vector Math

```qd
use math

struct Vec2 {
	x:f64
	y:f64
}

fn vec2_new(x:f64 y:f64 -- v:ptr) {
	-> y -> x
	Vec2 { x = x y = y }
}

fn vec2_add(a:ptr b:ptr -- result:ptr) {
	-> b -> a
	a @x b @x + -> x
	a @y b @y + -> y
	Vec2 { x = x y = y }
}

fn vec2_magnitude(v:ptr -- mag:f64) {
	-> v
	v @x dup *
	v @y dup * +
	math::sqrt
}

fn vec2_print(v:ptr -- ) {
	-> v
	"Vec2(" print v @x print ", " print v @y print ")" print nl
}

fn main() {
	3.0 4.0 vec2_new -> a
	1.0 2.0 vec2_new -> b

	"a = " print a vec2_print
	"b = " print b vec2_print

	a b vec2_add -> c
	"a + b = " print c vec2_print

	"magnitude of a: " print a vec2_magnitude print nl  // 5
	"magnitude of c: " print c vec2_magnitude print nl  // sqrt(16+36)
}
```

## Summary

Key concepts:

1. **Define structs** with `struct Name { field:type ... }`
2. **Create instances** with `Name { field = value ... }`
3. **Access fields** with `@fieldname`
4. **Modify fields** with `value struct .fieldname`
5. **Pass to functions** as `ptr` type
6. **Memory is managed** automatically or manually with `free`

## Next Steps

- **Next:** [Error Handling](tutorial-errors.md) - Working with fallible functions
- [Standard Library](stdlib/index.md) - Available modules and functions

