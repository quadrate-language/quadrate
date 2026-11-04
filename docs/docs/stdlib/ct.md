# `use` ct

<!-- doccheck: page-context use ct -->

<!-- doccheck: page-setup ct::Vec<i64> { data = null len = 0 cap = 0 } -> v -->

<!-- doccheck: page-setup ct::Map<i64> { keys = null values = null states = null len = 0 cap = 0 } -> m -->

<!-- doccheck: page-setup ct::Set { keys = null states = null len = 0 cap = 0 } -> s -->

<!-- doccheck: page-setup ct::Queue<i64> { data = null head = 0 tail = 0 len = 0 cap = 0 } -> q -->

<!-- doccheck: page-setup ct::Deque<i64> { data = null head = 0 tail = 0 len = 0 cap = 0 } -> d -->

<!-- doccheck: page-setup ct::Pair<i64, str> { first = 42 second = "hello" } -> p -->

Generic container types: Vec, Map, Set, Queue, Deque and Pair.

A container is created with a struct literal whose fields are all zero or null; there is no
constructor function, and the first insertion allocates. A method that changes a container
returns it, and that result must be used: chain the next call on it (`10 push! 20 push!`)
or re-bind it (`v 10 push! -> v`). Call `release` on a Vec, Map, Set, Queue or Deque when
done to free its memory.

**Example:**

```qd
use ct

fn main() {
	ct::Vec<i64> { data = null len = 0 cap = 0 }
	10 push! 20 push!
	dup 0 get! print nl                     // 10
	release

	ct::Map<i64> { keys = null values = null states = null len = 0 cap = 0 }
	"alice" 42 insert!
	dup "alice" get! print nl               // 42
	release

	ct::Set { keys = null states = null len = 0 cap = 0 }
	"alice" add!
	dup "alice" contains print nl           // 1
	release

	ct::Queue<i64> { data = null head = 0 tail = 0 len = 0 cap = 0 }
	1 enqueue! 2 enqueue!
	dequeue! swap print nl                  // 1
	release

	ct::Deque<i64> { data = null head = 0 tail = 0 len = 0 cap = 0 }
	1 push_back! 0 push_front!
	dup peek_front! print nl                // 0
	release

	ct::Pair<i64, str> { first = 42 second = "hello" }
	unpack swap print " " print print nl    // 42 hello
}
```

## Deque

Double-ended queue with elements of type T.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `data` | `ptr` | Pointer to circular buffer |
| `head` | `i64` | Index of front element |
| `tail` | `i64` | Index after last element |
| `len` | `i64` | Number of elements |
| `cap` | `i64` | Buffer capacity |

### Methods

#### `fn` is_empty

Check if the deque is empty.

**Signature:** `(d:Deque<T>) is_empty<T>( -- b:i64)`

**Example:**

```qd
d is_empty if { "empty!" print nl }
```

---

#### `fn` length

Get the number of elements.

**Signature:** `(d:Deque<T>) length<T>( -- n:i64)`

**Example:**

```qd
d length  // n
```

---

#### `fn` peek_back

View the back element without removing it.

**Signature:** `(d:Deque<T>) peek_back<T>( -- val:T)!`

**Example:**

```qd
d peek_back!  // val
```

---

#### `fn` peek_front

View the front element without removing it.

**Signature:** `(d:Deque<T>) peek_front<T>( -- val:T)!`

**Example:**

```qd
d peek_front!  // val
```

---

#### `fn` pop_back

Remove and return the back element.

**Signature:** `(d:Deque<T>) pop_back<T>( -- val:T d2:Deque<T>)!`

**Example:**

```qd
d pop_back!  // val d2
```

---

#### `fn` pop_front

Remove and return the front element.

**Signature:** `(d:Deque<T>) pop_front<T>( -- val:T d2:Deque<T>)!`

**Example:**

```qd
d pop_front!  // val d2
```

---

#### `fn` push_back

Push an element to the back.

**Signature:** `(d:Deque<T>) push_back<T>(val:T -- d2:Deque<T>)!`

**Example:**

```qd
d 42 push_back! -> d  // re-bind the returned deque
```

---

#### `fn` push_front

Push an element to the front.

**Signature:** `(d:Deque<T>) push_front<T>(val:T -- d2:Deque<T>)!`

**Example:**

```qd
d 42 push_front! -> d  // re-bind the returned deque
```

---

#### `fn` release

Free the deque's memory.

**Signature:** `(d:Deque<T>) release<T>()`

**Example:**

```qd
d release
```

## Map

Hash map with string keys and values of any type V -- i64, f64, str, ptr or a struct. A value lives in a tagged slot (mem::set_any), so it reads back as what was inserted, and the map holds a reference to a string or a struct for as long as the entry is in it.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `keys` | `ptr` | Array of string key pointers |
| `values` | `ptr` | Array of V values |
| `states` | `ptr` | Array of slot states (0=empty, 1=occupied, 2=deleted) |
| `len` | `i64` | Number of entries |
| `cap` | `i64` | Allocated capacity |

### Methods

#### `fn` get

Get the value for a key. Returns an error if the key is not found.

**Signature:** `(m:Map<V>) get<V>(key:str -- value:V)!`

**Example:**

```qd
m "alice" get!  // age
```

---

#### `fn` has

Check if a key exists in the map.

**Signature:** `(m:Map<V>) has<V>(key:str -- exists:i64)`

**Example:**

```qd
m "alice" has if { "found!" print nl }
```

---

#### `fn` insert

Insert a key-value pair into the map. If the key already exists, the value is updated. Returns the updated map which must be captured.

**Signature:** `(m:Map<V>) insert<V>(key:str value:V -- m2:Map<V>)!`

**Example:**

```qd
m "alice" 42 insert! -> m  // re-bind the returned map
```

---

#### `fn` is_empty

Check if the map is empty.

**Signature:** `(m:Map<V>) is_empty<V>( -- b:i64)`

**Example:**

```qd
m is_empty if { "empty!" print nl }
```

---

#### `fn` length

Get the number of entries in the map.

**Signature:** `(m:Map<V>) length<V>( -- n:i64)`

**Example:**

```qd
m length  // n
```

---

#### `fn` release

Free the map's memory. IMPORTANT: Always call this when done with a map to prevent memory leaks. After calling release, the map should not be used.

**Signature:** `(m:Map<V>) release<V>()`

**Example:**

```qd
m release
```

---

#### `fn` remove

Remove a key from the map. Returns error if the key is not found. Returns the updated map which must be captured.

**Signature:** `(m:Map<V>) remove<V>(key:str -- m2:Map<V>)!`

**Example:**

```qd
m "alice" remove! -> m  // re-bind the returned map
```

## Pair

A pair of two values.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `first` | `T` | The first element of type T |
| `second` | `U` | The second element of type U |

### Methods

#### `fn` first

Get the first element of a pair.

**Signature:** `(p:Pair<T, U>) first<T, U>( -- a:T)`

**Example:**

```qd
p first  // a
```

---

#### `fn` second

Get the second element of a pair.

**Signature:** `(p:Pair<T, U>) second<T, U>( -- b:U)`

**Example:**

```qd
p second  // b
```

---

#### `fn` unpack

Unpack a pair onto the stack. Returns both elements: first is deeper, second on top.

**Signature:** `(p:Pair<T, U>) unpack<T, U>( -- a:T b:U)`

**Example:**

```qd
p unpack  // a b
```

## Queue

FIFO Queue with elements of type T.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `data` | `ptr` | Pointer to circular buffer |
| `head` | `i64` | Index of front element |
| `tail` | `i64` | Index after last element |
| `len` | `i64` | Number of elements |
| `cap` | `i64` | Buffer capacity |

### Methods

#### `fn` dequeue

Remove and return the front element. Returns error if queue is empty. Returns the value and updated queue.

**Signature:** `(q:Queue<T>) dequeue<T>( -- val:T q2:Queue<T>)!`

**Example:**

```qd
q dequeue!  // val q2
```

---

#### `fn` enqueue

Add an element to the back of the queue. Returns the updated queue which must be captured.

**Signature:** `(q:Queue<T>) enqueue<T>(val:T -- q2:Queue<T>)!`

**Example:**

```qd
q 42 enqueue! -> q  // re-bind the returned queue
```

---

#### `fn` is_empty

Check if the queue is empty.

**Signature:** `(q:Queue<T>) is_empty<T>( -- b:i64)`

**Example:**

```qd
q is_empty if { "empty!" print nl }
```

---

#### `fn` length

Get the number of elements in the queue.

**Signature:** `(q:Queue<T>) length<T>( -- n:i64)`

**Example:**

```qd
q length  // n
```

---

#### `fn` peek

View the front element without removing it. Returns error if queue is empty.

**Signature:** `(q:Queue<T>) peek<T>( -- val:T)!`

**Example:**

```qd
q peek!  // val
```

---

#### `fn` release

Free the queue's memory.

**Signature:** `(q:Queue<T>) release<T>()`

**Example:**

```qd
q release
```

## Set

Set of unique string elements.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `keys` | `ptr` | Array of string key pointers |
| `states` | `ptr` | Array of slot states (0=empty, 1=occupied, 2=deleted) |
| `len` | `i64` | Number of elements |
| `cap` | `i64` | Allocated capacity |

### Methods

#### `fn` add

Add an element to the set. If the element already exists, the set is unchanged. Returns the updated set which must be captured.

**Signature:** `(s:Set) add(key:str -- s2:Set)!`

**Example:**

```qd
s "alice" add! -> s  // re-bind the returned set
```

---

#### `fn` contains

Check if an element exists in the set.

**Signature:** `(s:Set) contains(key:str -- exists:i64)`

**Example:**

```qd
s "alice" contains if { "found!" print nl }
```

---

#### `fn` is_empty

Check if the set is empty.

**Signature:** `(s:Set) is_empty( -- b:i64)`

**Example:**

```qd
s is_empty if { "empty!" print nl }
```

---

#### `fn` length

Get the number of elements in the set.

**Signature:** `(s:Set) length( -- n:i64)`

**Example:**

```qd
s length  // n
```

---

#### `fn` release

Free the set's memory. IMPORTANT: Always call this when done with a set to prevent memory leaks.

**Signature:** `(s:Set) release()`

**Example:**

```qd
s release
```

---

#### `fn` remove

Remove an element from the set. Returns error if the element is not found. Returns the updated set which must be captured.

**Signature:** `(s:Set) remove(key:str -- s2:Set)!`

**Example:**

```qd
s "alice" remove! -> s  // re-bind the returned set
```

## Vec

Dynamic array (vector) structure.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `data` | `ptr` | Pointer to contiguous element storage |
| `len` | `i64` | Current number of elements (0 to cap) |
| `cap` | `i64` | Allocated capacity in elements |

### Methods

#### `fn` capacity

Get the allocated capacity. Capacity is always >= length. The vector can hold up to 'capacity' elements before needing to reallocate.

**Signature:** `(v:Vec<T>) capacity<T>( -- n:i64)`

**Example:**

```qd
v capacity  // cap
```

---

#### `fn` get

Get element at index. Panics if index is out of bounds (< 0 or >= len).

**Signature:** `(v:Vec<T>) get<T>(idx:i64 -- elem:T)!`

**Example:**

```qd
v 0 get!  // first
```

---

#### `fn` is_empty

Check if the vector is empty (length == 0).

**Signature:** `(v:Vec<T>) is_empty<T>( -- b:i64)`

**Example:**

```qd
v is_empty if { "empty!" print nl }
```

---

#### `fn` length

Get the number of elements in the vector.

**Signature:** `(v:Vec<T>) length<T>( -- n:i64)`

**Example:**

```qd
v length  // n
```

---

#### `fn` pop

Pop and return the last element. Panics if the vector is empty. Returns both the element and the updated vector.

**Signature:** `(v:Vec<T>) pop<T>( -- elem:T v2:Vec<T>)!`

**Example:**

```qd
v pop!  // elem v2
```

---

#### `fn` push

Push an element to the end of the vector. Automatically grows capacity when full (doubles size, starting at 8). Returns the updated vector which must be captured.

**Signature:** `(v:Vec<T>) push<T>(elem:T -- v2:Vec<T>)!`

**Example:**

```qd
v 42 push! -> v  // re-bind the returned vector
```

---

#### `fn` release

Free the vector's memory. IMPORTANT: Always call this when done with a vector to prevent memory leaks. After calling release, the vector should not be used.

**Signature:** `(v:Vec<T>) release<T>()`

**Example:**

```qd
v release
```

---

#### `fn` reset

Remove all elements, keeping allocated capacity. Useful for reusing a vector without reallocating.

**Signature:** `(v:Vec<T>) reset<T>( -- v2:Vec<T>)`

**Example:**

```qd
v reset -> v  // re-bind the returned vector
```

---

#### `fn` set

Set element at index. Panics if index is out of bounds (< 0 or >= len). Returns the updated vector which must be captured.

**Signature:** `(v:Vec<T>) set<T>(idx:i64 elem:T -- v2:Vec<T>)!`

**Example:**

```qd
v 0 100 set! -> v  // re-bind the returned vector
```

