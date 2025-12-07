# Example: Data Structures

Implementing common data structures in Quadrate.

## Stack (LIFO)

```qd
use mem

struct Stack {
	data:ptr
	top:i64
	capacity:i64
}

fn stack_new(capacity:i64 -- s:ptr) {
	-> capacity
	capacity 8 * mem::alloc -> data
	Stack { data = data top = 0 capacity = capacity }
}

fn stack_free(s:ptr -- ) {
	-> s
	s @data mem::free
}

fn stack_push(s:ptr value:i64 -- ) {
	-> value -> s
	s @top s @capacity >= if {
		"Stack overflow" print nl
	} else {
		value s @data s @top 8 * + mem::write_i64
		s @top 1 + s !top
	}
}

fn stack_pop(s:ptr -- value:i64) {
	-> s
	s @top 0 <= if {
		"Stack underflow" print nl
		0
	} else {
		s @top 1 - s !top
		s @data s @top 8 * + mem::read_i64
	}
}

fn stack_is_empty(s:ptr -- empty:i64) {
	-> s
	s @top 0 ==
}

fn stack_peek(s:ptr -- value:i64) {
	-> s
	s @top 0 <= if {
		0
	} else {
		s @data s @top 1 - 8 * + mem::read_i64
	}
}

fn main( -- ) {
	10 stack_new -> s
	defer { s stack_free }

	s 10 stack_push
	s 20 stack_push
	s 30 stack_push

	s stack_pop print nl  // 30
	s stack_pop print nl  // 20
	s stack_pop print nl  // 10
}
```

## Queue (FIFO)

```qd
use mem

struct Queue {
	data:ptr
	head:i64
	tail:i64
	size:i64
	capacity:i64
}

fn queue_new(capacity:i64 -- q:ptr) {
	-> capacity
	capacity 8 * mem::alloc -> data
	Queue { data = data head = 0 tail = 0 size = 0 capacity = capacity }
}

fn queue_free(q:ptr -- ) {
	-> q
	q @data mem::free
}

fn queue_enqueue(q:ptr value:i64 -- ) {
	-> value -> q
	q @size q @capacity >= if {
		"Queue full" print nl
	} else {
		value q @data q @tail 8 * + mem::write_i64
		q @tail 1 + q @capacity % q !tail
		q @size 1 + q !size
	}
}

fn queue_dequeue(q:ptr -- value:i64) {
	-> q
	q @size 0 <= if {
		"Queue empty" print nl
		0
	} else {
		q @data q @head 8 * + mem::read_i64 -> value
		q @head 1 + q @capacity % q !head
		q @size 1 - q !size
		value
	}
}

fn queue_is_empty(q:ptr -- empty:i64) {
	-> q
	q @size 0 ==
}

fn main( -- ) {
	10 queue_new -> q
	defer { q queue_free }

	q 1 queue_enqueue
	q 2 queue_enqueue
	q 3 queue_enqueue

	q queue_dequeue print nl  // 1
	q queue_dequeue print nl  // 2
	q queue_dequeue print nl  // 3
}
```

## Linked List

```qd
struct Node {
	value:i64
	next:ptr
}

struct LinkedList {
	head:ptr
	tail:ptr
	length:i64
}

fn list_new( -- list:ptr) {
	LinkedList { head = 0 tail = 0 length = 0 }
}

fn list_append(list:ptr value:i64 -- ) {
	-> value -> list
	Node { value = value next = 0 } -> node

	list @head 0 == if {
		node list !head
		node list !tail
	} else {
		node list @tail !next
		node list !tail
	}
	list @length 1 + list !length
}

fn list_prepend(list:ptr value:i64 -- ) {
	-> value -> list
	Node { value = value next = list @head } -> node
	node list !head
	list @tail 0 == if {
		node list !tail
	}
	list @length 1 + list !length
}

fn list_foreach(list:ptr callback:ptr -- ) {
	-> callback -> list
	list @head -> current
	current 0 != while {
		current @value callback call
		current @next -> current
	}
}

fn print_value(v:i64 -- ) {
	-> v
	v print " " print
}

fn main( -- ) {
	list_new -> list

	list 10 list_append
	list 20 list_append
	list 30 list_append
	list 5 list_prepend

	list &print_value list_foreach
	nl  // 5 10 20 30
}
```

## Hash Map (Simple)

```qd
use mem

const MAP_SIZE 256

struct Entry {
	key:str
	value:i64
	next:ptr
}

struct HashMap {
	buckets:ptr
}

fn hash(key:str -- h:i64) {
	-> key
	0 -> h
	// Simple hash
	key iter for c {
		h 31 * c + -> h
	}
	h abs MAP_SIZE %
}

fn map_new( -- m:ptr) {
	MAP_SIZE 8 * mem::alloc -> buckets
	buckets 0 MAP_SIZE 8 * mem::fill
	HashMap { buckets = buckets }
}

fn map_put(m:ptr key:str value:i64 -- ) {
	-> value -> key -> m
	key hash -> idx

	Entry { key = key value = value next = 0 } -> entry

	m @buckets idx 8 * + mem::read_ptr -> existing
	existing 0 != if {
		existing entry !next
	}
	entry m @buckets idx 8 * + mem::write_ptr
}

fn map_get(m:ptr key:str -- value:i64 found:i64) {
	-> key -> m
	key hash -> idx

	m @buckets idx 8 * + mem::read_ptr -> current
	0 -> found
	0 -> value

	current 0 != found not and while {
		current @key key str::eq if {
			current @value -> value
			1 -> found
		} else {
			current @next -> current
		}
	}

	value found
}

fn main( -- ) {
	map_new -> m

	m "apple" 5 map_put
	m "banana" 3 map_put
	m "cherry" 7 map_put

	m "banana" map_get if {
		"banana: " print print nl  // 3
	} else {
		drop
		"Not found" print nl
	}

	m "grape" map_get if {
		"grape: " print print nl
	} else {
		drop
		"grape not found" print nl
	}
}
```

## Binary Tree

```qd
struct TreeNode {
	value:i64
	left:ptr
	right:ptr
}

fn tree_insert(root:ptr value:i64 -- new_root:ptr) {
	-> value -> root

	root 0 == if {
		TreeNode { value = value left = 0 right = 0 }
	} else {
		value root @value < if {
			root @left value tree_insert root !left
		} else {
			root @right value tree_insert root !right
		}
		root
	}
}

fn tree_inorder(node:ptr callback:ptr -- ) {
	-> callback -> node

	node 0 != if {
		node @left callback tree_inorder
		node @value callback call
		node @right callback tree_inorder
	}
}

fn tree_contains(node:ptr value:i64 -- found:i64) {
	-> value -> node

	node 0 == if {
		0
	} else {
		node @value value == if {
			1
		} else {
			value node @value < if {
				node @left value tree_contains
			} else {
				node @right value tree_contains
			}
		}
	}
}

fn print_value(v:i64 -- ) {
	-> v
	v print " " print
}

fn main( -- ) {
	0 -> root

	root 50 tree_insert -> root
	root 30 tree_insert -> root
	root 70 tree_insert -> root
	root 20 tree_insert -> root
	root 40 tree_insert -> root
	root 60 tree_insert -> root
	root 80 tree_insert -> root

	"In-order: " print
	root &print_value tree_inorder
	nl  // 20 30 40 50 60 70 80

	"Contains 40: " print root 40 tree_contains print nl  // 1
	"Contains 45: " print root 45 tree_contains print nl  // 0
}
```

## Key Takeaways

1. **Structs for nodes** - Each data structure uses structs for elements
2. **Pointers link elements** - Use `ptr` fields to connect nodes
3. **Memory management** - Allocate and free carefully
4. **Function pointers for callbacks** - Enable generic traversal

## Next Steps

- Explore the [Standard Library](../../stdlib/index.md) for built-in utilities
- Check out real-world [Examples](https://git.sr.ht/~klahr/quadrate/tree/master/item/examples)
