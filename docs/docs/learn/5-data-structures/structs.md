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
	p <<x print nl  // 10
	p <<y print nl  // 20
}
```

## Reading fields

Use `<<fieldname` to read:

```qd
struct Rectangle {
	width:f64
	height:f64
}

fn area(rect:Rectangle -- a:f64) {
	rect <<width rect <<height *
}

fn main() {
	Rectangle {
		width = 5.0
		height = 3.0
	} -> rect
	rect area print nl  // 15
}
```

### Type narrowing with as

When working with raw `ptr` values (common in callbacks), use `as` to tell the compiler the struct type:

```qd
fn handler(data:ptr -- ) {
	data as Config <<debug print nl
}
```

This is compile-time only — no runtime cost. It's required when multiple structs share a field name, as the compiler cannot guess which one you mean.

## Writing fields

Use `>>fieldname` to write. Two variants:

- `>>field` — pushes modified struct back (for chaining)
- `>>field!` — discards struct (for standalone mutation)

```qd
struct Counter {
	value:i64
}

fn main() {
	Counter {
		value = 0
	} -> c

	c <<value print nl  // 0

	// >>field! for standalone mutation
	c 10 >>value!
	c <<value print nl  // 10

	// >>field with rebind
	c c <<value inc >>value -> c
	c <<value print nl  // 11
}
```

Chain writes during construction:
```qd
Point { x = 0.0 y = 0.0 } 1.0 >>x 2.0 >>y -> p
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

	line <<start <<x print nl  // 0
	line <<end <<x print nl    // 10
}
```

## Structs with arrays

```qd
struct Point {
	x:f64
	y:f64
}

struct Polygon {
	points:[]Point
}

fn main() {
	3 make<Point> -> pts
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
	} -> triangle

	triangle <<points len print nl  // 3
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
	p <<x dup * p <<y dup * + math::sqrt
}

fn (p:Point) move(dx:f64 dy:f64 -- ) {
	p p <<x dx + >>x -> p
	p p <<y dy + >>y -> p
}

fn main() {
	Point { x = 3.0 y = 4.0 } -> pt

	// Call method on struct - receiver is popped from stack
	pt magnitude print nl  // 5

	// Methods can also work on inline struct construction
	Point { x = 5.0 y = 12.0 } magnitude print nl  // 13

	// Method with parameters
	pt 1.0 1.0 move
	pt <<x print nl  // 4
}
```

### Method resolution

When a struct is on the stack and you use an identifier, methods take precedence over global functions:

```qd
struct Counter {
	value:i64
}

fn (c:Counter) bump( -- result:i64) {
	c <<value 1 +
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
	p2 <<x p1 <<x - dup *
	p2 <<y p1 <<y - dup *
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
	cfg value >>debug -> cfg
	cfg
}

fn main() {
	config_new 1 config_set_debug -> cfg
	cfg <<debug print nl  // 1
}
```

### Linked list node

```qd
struct Node {
	value:i64
	next:ptr
}

fn node_new(value:i64 -- node:ptr) {
	Node {
		value = value
		next = 0
	}
}

fn main() {
	10 node_new -> first
	20 node_new -> second
	30 node_new -> third

	first second >>next -> first
	second third >>next -> second

	// Traverse
	first -> current
	loop {
		current 0 == if { break }
		current <<value print nl
		current <<next -> current
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
	capacity make<i64> -> data
	Stack {
		data = data
		top = 0
		capacity = capacity
	}
}

fn stack_push(s:Stack value:i64 -- ) {
	s <<data s <<top value set
	s s <<top 1 + >>top -> s
}

fn stack_pop(s:Stack -- value:i64) {
	s s <<top 1 - >>top -> s
	s <<data s <<top nth
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
	a <<x b <<x == a <<y b <<y == and
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

## Pointer fields and recursive structs

Structs can contain pointers to other structs using `*StructName`. This enables linked lists, trees, and other recursive data structures.

```qd
struct Node {
	value:i64
	next:*Node
}

fn main() {
	// Build a linked list: 3 -> 2 -> 1 -> null
	Node { value = 1 next = null } -> a
	Node { value = 2 next = a } -> b
	Node { value = 3 next = b } -> c

	// Traverse using <<field access through pointers
	c <<value print nl         // 3
	c <<next <<value print nl   // 2
}
```

Use `null` for empty pointer fields. Check for null before accessing:

```qd
fn print_list(cur:ptr -- ) {
	loop {
		cur null eq if { break }
		cur <<value print nl
		cur <<next -> cur
	}
}
```

### Binary tree example

```qd
struct TreeNode {
	value:i64
	left:*TreeNode
	right:*TreeNode
}

fn inorder(n:ptr -- ) {
	n null neq if {
		n <<left inorder
		n <<value print nl
		n <<right inorder
	}
}
```

## Packed structs and sized integers

By default, struct fields are rounded up to 8-byte slots so that pointers, `i64`, and `f64` all share one alignment rule. When you need to parse a binary format — WAD lumps, PNG chunks, network headers — you can declare a **packed struct** that lays fields out back-to-back at their exact widths, plus use **sized integer types** (`i8/i16/i32/u8/u16/u32/u64`) to match the on-disk representation exactly.

```qd
packed struct FileLump {
	offset: u32
	size:   u32
	name:   u64       // 8 raw bytes
}
// sizeof(FileLump) == 16 bytes (a plain struct would be 24).

fn main() {
	FileLump { offset = 100 size = 200 name = 0 } -> lump
	lump <<offset print nl      // 100
	lump <<size   print nl      // 200
}
```

Sized integer fields store at their declared width in memory and sign-extend (signed) or zero-extend (unsigned) to `i64` when pushed onto the stack. See [Type casting](../../reference/types.md#sized-integer-types) for the details and the [keywords reference](../../reference/keywords.md#packed) for `packed`.

## What's next?

Learn about [Constants & Enums](constants.md) to define fixed values.
