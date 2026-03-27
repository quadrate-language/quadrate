# `use` ct

Container types for Quadrate: dynamic arrays, queues, sets, hash maps, and pairs.

All container types manage their own memory. Mutating operations return an
updated copy of the container that must be captured with `->`. Always call
`release` when you are done with a container to free its memory.

```qd
use ct

Vec<i64> { data = 0 len = 0 cap = 0 } -> v
v 10 push! -> v
v 20 push! -> v
v 0 get! print nl  // 10
v release
```

---

## Vec\<T\> - Dynamic Array

A growable array that doubles in capacity when full.

```qd
pub struct Vec<T> {
	data: ptr
	len: i64
	cap: i64
}
```

Create an empty vector:

```qd
Vec<i64> { data = 0 len = 0 cap = 0 } -> v
```

### `fn` push

Append an element. Grows capacity automatically.

**Signature:** `(v:Vec<T> elem:T -- v2:Vec<T>)!`

**Example:**

```qd
v 42 push! -> v
```

---

### `fn` pop

Remove and return the last element.

**Signature:** `(v:Vec<T> -- elem:T v2:Vec<T>)!`

**Example:**

```qd
v pop! -> v -> elem
```

---

### `fn` get

Get element at index.

**Signature:** `(v:Vec<T> idx:i64 -- elem:T)!`

**Example:**

```qd
v 0 get! -> first
```

---

### `fn` set

Replace element at index.

**Signature:** `(v:Vec<T> idx:i64 elem:T -- v2:Vec<T>)!`

**Example:**

```qd
v 0 100 set! -> v
```

---

### `fn` length

**Signature:** `(v:Vec<T> -- n:i64)`

```qd
v length -> n
```

---

### `fn` capacity

Get allocated capacity (may be larger than length).

**Signature:** `(v:Vec<T> -- n:i64)`

---

### `fn` is_empty

**Signature:** `(v:Vec<T> -- b:i64)`

Returns 1 if the vector has no elements.

---

### `fn` reset

Remove all elements but keep allocated memory.

**Signature:** `(v:Vec<T> -- v2:Vec<T>)`

```qd
v reset -> v
v length print nl  // 0
```

---

### `fn` release

Free the vector's memory. Always call this when done.

**Signature:** `(v:Vec<T> --)`

---

## Deque\<T\> - Double-Ended Queue

O(1) push and pop from both ends, backed by a circular buffer.

```qd
pub struct Deque<T> {
	data: ptr
	head: i64
	tail: i64
	len: i64
	cap: i64
}
```

Create an empty deque:

```qd
Deque<i64> { data = 0 head = 0 tail = 0 len = 0 cap = 0 } -> d
```

### `fn` push_back

**Signature:** `(d:Deque<T> val:T -- d2:Deque<T>)!`

```qd
d 42 push_back! -> d
```

---

### `fn` push_front

**Signature:** `(d:Deque<T> val:T -- d2:Deque<T>)!`

```qd
d 10 push_front! -> d
```

---

### `fn` pop_front

Remove and return the front element.

**Signature:** `(d:Deque<T> -- val:T d2:Deque<T>)!`

```qd
d pop_front! -> d -> val
```

---

### `fn` pop_back

Remove and return the back element.

**Signature:** `(d:Deque<T> -- val:T d2:Deque<T>)!`

```qd
d pop_back! -> d -> val
```

---

### `fn` peek_front

View front element without removing it.

**Signature:** `(d:Deque<T> -- val:T)!`

---

### `fn` peek_back

View back element without removing it.

**Signature:** `(d:Deque<T> -- val:T)!`

---

### `fn` length

**Signature:** `(d:Deque<T> -- n:i64)`

---

### `fn` is_empty

**Signature:** `(d:Deque<T> -- b:i64)`

---

### `fn` release

**Signature:** `(d:Deque<T> --)`

---

## Queue\<T\> - FIFO Queue

First-in, first-out queue backed by a circular buffer.

```qd
pub struct Queue<T> {
	data: ptr
	head: i64
	tail: i64
	len: i64
	cap: i64
}
```

Create an empty queue:

```qd
Queue<i64> { data = 0 head = 0 tail = 0 len = 0 cap = 0 } -> q
```

### `fn` enqueue

Add an element to the back of the queue.

**Signature:** `(q:Queue<T> val:T -- q2:Queue<T>)!`

**Example:**

```qd
q 42 enqueue! -> q
```

---

### `fn` dequeue

Remove and return the front element.

**Signature:** `(q:Queue<T> -- val:T q2:Queue<T>)!`

**Example:**

```qd
q dequeue! -> q -> val
```

---

### `fn` peek

View front element without removing it.

**Signature:** `(q:Queue<T> -- val:T)!`

---

### `fn` length

**Signature:** `(q:Queue<T> -- n:i64)`

---

### `fn` is_empty

**Signature:** `(q:Queue<T> -- b:i64)`

---

### `fn` release

**Signature:** `(q:Queue<T> --)`

---

## Set - String Set

A set of unique strings with O(1) average-case lookup.
Uses open addressing with linear probing and tombstone deletion.

```qd
pub struct Set {
	keys: ptr
	states: ptr
	len: i64
	cap: i64
}
```

Create an empty set:

```qd
Set { keys = 0 states = 0 len = 0 cap = 0 } -> s
```

### `fn` add

Add a string element. Duplicates are silently ignored.

**Signature:** `(s:Set key:str -- s2:Set)!`

**Example:**

```qd
s "alice" add! -> s
s "alice" add! -> s  // no effect, already present
```

---

### `fn` contains

Check if a string is in the set.

**Signature:** `(s:Set key:str -- exists:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `exists` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
s "alice" contains if { "found" print nl }
```

---

### `fn` remove

Remove a string from the set. Errors if not found.

**Signature:** `(s:Set key:str -- s2:Set)!`

---

### `fn` length

**Signature:** `(s:Set -- n:i64)`

---

### `fn` is_empty

**Signature:** `(s:Set -- b:i64)`

---

### `fn` release

**Signature:** `(s:Set --)`

---

## Map\<V\> - String-Keyed Hash Map

A hash map where keys are strings and values can be any type V.
O(1) average-case lookup using open addressing with linear probing.

```qd
pub struct Map<V> {
	keys: ptr
	values: ptr
	states: ptr
	len: i64
	cap: i64
}
```

Create an empty map:

```qd
Map<i64> { keys = 0 values = 0 states = 0 len = 0 cap = 0 } -> m
```

### `fn` insert

Insert or update a key-value pair.

**Signature:** `(m:Map<V> key:str value:V -- m2:Map<V>)!`

**Example:**

```qd
m "alice" 42 insert! -> m
m "alice" 99 insert! -> m  // overwrites, length stays 1
```

---

### `fn` get

Look up a value by key. Errors if the key does not exist.

**Signature:** `(m:Map<V> key:str -- value:V)!`

**Example:**

```qd
m "alice" get! -> age
```

---

### `fn` has

Check if a key exists without retrieving its value.

**Signature:** `(m:Map<V> key:str -- exists:i64)`

| Output | Type | Description |
|--------|------|-------------|
| `exists` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
m "alice" has if { "found" print nl }
```

---

### `fn` remove

Remove a key. Errors if the key does not exist.

**Signature:** `(m:Map<V> key:str -- m2:Map<V>)!`

---

### `fn` length

**Signature:** `(m:Map<V> -- n:i64)`

---

### `fn` is_empty

**Signature:** `(m:Map<V> -- b:i64)`

---

### `fn` release

**Signature:** `(m:Map<V> --)`

---

## Pair\<T, U\> - Two-Element Tuple

A generic pair holding two values of possibly different types.

```qd
pub struct Pair<T, U> {
	first: T
	second: U
}
```

Create a pair:

```qd
Pair<i64, i64> { first = 42 second = 99 } -> p
```

### Accessing fields

Read fields with `@`:

```qd
p <<first -> a
p <<second -> b
```

### `fn` first

**Signature:** `(p:Pair<T,U> -- a:T)`

### `fn` second

**Signature:** `(p:Pair<T,U> -- b:U)`

### `fn` unpack

Get both elements at once.

**Signature:** `(p:Pair<T,U> -- a:T b:U)`

**Example:**

```qd
p unpack -> b -> a
```

---

## Complete example

```qd
use ct

fn main() {
	// Build a Vec of scores
	Vec<i64> { data = 0 len = 0 cap = 0 } -> scores
	scores 10 push! -> scores
	scores 20 push! -> scores
	scores 30 push! -> scores
	scores 1 get! print nl  // 20
	scores release

	// Use a Deque as a sliding window
	Deque<i64> { data = 0 head = 0 tail = 0 len = 0 cap = 0 } -> window
	window 1 push_back! -> window
	window 2 push_back! -> window
	window 3 push_back! -> window
	window pop_front! -> window -> oldest
	oldest print nl  // 1
	window release

	// Process jobs in order with a Queue
	Queue<i64> { data = 0 head = 0 tail = 0 len = 0 cap = 0 } -> jobs
	jobs 100 enqueue! -> jobs
	jobs 200 enqueue! -> jobs
	jobs dequeue! -> jobs -> job
	job print nl  // 100
	jobs release

	// Track unique visitors with a Set
	Set { keys = 0 states = 0 len = 0 cap = 0 } -> visitors
	visitors "alice" add! -> visitors
	visitors "bob" add! -> visitors
	visitors "alice" add! -> visitors  // duplicate, ignored
	visitors length print nl  // 2
	visitors release

	// Store ages in a Map
	Map<i64> { keys = 0 values = 0 states = 0 len = 0 cap = 0 } -> ages
	ages "alice" 30 insert! -> ages
	ages "bob" 25 insert! -> ages
	ages "alice" get! print nl  // 30
	ages release

	// Bundle two values with a Pair
	Pair<i64, i64> { first = 640 second = 480 } -> size
	size unpack -> h -> w
	w print " x " print h print nl  // 640 x 480
}
```
