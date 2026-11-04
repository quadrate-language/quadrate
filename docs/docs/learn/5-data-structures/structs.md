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
	}
	dup <<x print nl  // 10
	<<y print nl      // 20
}
```

### From the stack

A struct's name used as a word builds one from values already on the stack, one per field in
the order the struct declares them. It is the stack-style constructor: a function whose inputs
are the fields needs no body but the name.

```qd
struct Point {
	x:f64
	y:f64
}

fn new_point(x:f64 y:f64 -- p:Point) {
	Point
}

fn main() {
	3.0 4.0 Point <<y print nl  // 4
	1.0 2.0 new_point <<x print nl  // 1
}
```

Every field has to be given, defaults included; use the named form when only some are, or when
the values are not already on the stack in field order.

## Reading fields

Use `<<fieldname` to read:

```qd
struct Rectangle {
	width:f64
	height:f64
}

fn area(rect:Rectangle -- a:f64) {
	dup <<width swap <<height *
}

fn main() {
	Rectangle {
		width = 5.0
		height = 3.0
	} area print nl  // 15
}
```

### Structs are passed with their type

A parameter that takes a struct is declared with that struct's type. A `ptr` is raw memory --
what `mem::alloc` hands back -- and never holds a struct, so a struct passed where a `ptr` is
taken is a compile error:

```qd
struct Config {
	debug:i64
}

fn handler(data:Config -- ) {
	<<debug print nl
}

fn main() {
	Config { debug = 1 } handler   // 1
}
```

## Writing fields

Use `>>fieldname` to write. Two variants:

- `>>field` — writes the field and pushes the same struct back, so writes chain
- `>>field drop` — writes the field, then discards the struct reference

A struct is a reference, and `>>field` writes through it rather than producing a new
struct. Two names for one struct see each other's writes:

```qd
c -> same
same 10 >>value drop
c <<value print nl  // 10 — written through `same`, visible through `c`
```

`clone` is how you ask for an independent one:

```qd
c clone -> copy
copy 99 >>value drop
c <<value print nl     // 10 — untouched
copy <<value print nl  // 99
```

The copy is shallow, and the word means exactly one thing: a new struct of the same type
with the fields copied. What a field *points to* is shared, not copied. For a `str` field
that is invisible, because a string is immutable and a shared one cannot be told from a
copy. For an array or a nested struct it is visible, and deliberately so — a struct is a
reference, so a field holding one behaves on `clone` the same way `c -> same` does:

```qd
struct Tally { counts:[]i64  name:str }

Tally { counts = [0 0] name = "first" } -> t
t clone -> u
u <<counts 0 5 set
t <<counts 0 nth print nl  // 5 — the array is shared
```

To give the array its own copy, build one: `clone` never recurses, so there is no
question of how deep it goes and no way for it to loop on a structure that points at
itself.

```qd
struct Counter {
	value:i64
}

fn main() {
	Counter {
		value = 0
	} -> c

	c <<value print nl  // 0

	// >>field drop for standalone mutation
	c 10 >>value drop
	c <<value print nl  // 10

	// >>field with rebind
	c c <<value ++ >>value -> c
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
	0.0 0.0 Point
	10.0 10.0 Point
	Line

	dup <<start <<x print nl  // 0
	<<end <<x print nl        // 10
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
	[]Point
	Point {
		x = 0.0
		y = 0.0
	} append
	Point {
		x = 1.0
		y = 0.0
	} append
	Point {
		x = 0.5
		y = 1.0
	} append
	Polygon

	<<points len print nl  // 3
}
```

`[]Point` starts the array empty and `append` grows it. The sized form `[3]Point` is available
too, but it builds three real `Point {}` instances up front, so it needs every field of `Point`
to declare a default:

```qd
struct Point { x:f64 = 0.0  y:f64 = 0.0 }

fn main() {
	[3]Point
	dup 0 nth <<x print nl   // 0 -- a real Point, not an empty slot
	dup 1 nth 2.5 >>y drop   // each element is its own instance
	dup 0 nth <<y print nl   // 0
	1 nth <<y print nl       // 2.5
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
	p <<y + p swap >>y drop
	p <<x + p swap >>x drop
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
	Counter { value = 10 } bump print nl   // 11 (calls method)
	bump print nl                          // 999 (calls global function)
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

def point_distance(p1:Point p2:Point -- d:f64) {
	p2 <<x p1 <<x - dup *
	p2 <<y p1 <<y - dup *
	+ math::sqrt
}

fn main() {
	Point { x = 0.0 y = 0.0 }
	Point { x = 3.0 y = 4.0 }
	point_distance print nl  // 5
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
	>>debug
}

fn main() {
	config_new 1 config_set_debug <<debug print nl  // 1
}
```

### Linked list node

```qd
struct Node {
	value:i64
	next:*Node
}

fn node_new(value:i64 -- node:Node) {
	null Node
}

fn main() {
	10 node_new -> first
	20 node_new -> second
	30 node_new -> third

	first second >>next -> first
	second third >>next -> second

	// Traverse
	first
	loop {
		dup null != while
		dup <<value print nl
		<<next
	}
	drop
	// Output: 10 20 30, one per line
}
```

### Stack data structure

```qd
struct Stack {
	data:[]i64
	top:i64
	capacity:i64
}

def stack_new(capacity:i64 -- s:Stack) {
	[capacity]i64 0 capacity Stack
}

def stack_push(s:Stack value:i64 -- ) {
	s <<data s <<top value set
	s s <<top 1 + >>top -> s
}

fn stack_pop(s:Stack -- value:i64) {
	dup <<top 1 - >>top
	dup <<data swap <<top nth
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

def points_equal(a:Point b:Point -- equal:i64) {
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
	1 null Node
	2 swap Node
	3 swap Node

	// Traverse using <<field access through pointers
	dup <<value print nl      // 3
	<<next <<value print nl   // 2
}
```

Use `null` for empty pointer fields. Check for null before accessing:

```qd
struct Node {
	value:i64
	next:*Node
}

fn print_list(start:Node -- ) {
	loop {
		dup null != while
		dup <<value print nl
		<<next
	}
	drop
}
```

### Binary tree example

```qd
struct TreeNode {
	value:i64
	left:*TreeNode
	right:*TreeNode
}

fn inorder(n:TreeNode -- ) {
	dup null != if {
		dup <<left inorder
		dup <<value print nl
		<<right inorder
	} else {
		drop
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
	FileLump { offset = 100 size = 200 name = 0 }
	dup <<offset print nl      // 100
	<<size print nl            // 200
}
```

Sized integer fields store at their declared width in memory and sign-extend (signed) or zero-extend (unsigned) to `i64` when pushed onto the stack. See [Type casting](../../reference/types.md#sized-integer-types) for the details and the [keywords reference](../../reference/keywords.md#packed) for `packed`.

## What's next?

Learn about [Constants & Enums](constants.md) to define fixed values.
