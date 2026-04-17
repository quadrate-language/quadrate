# `use` ct

Generic container types.
Provides Vec, HashMap, Set, Queue, Deque, and Pair.

## Functions

### `fn` capacity

Get the allocated capacity.  Capacity is always >= length. The vector can hold up to 'capacity' elements before needing to reallocate.  Example:     v capacity -> cap  Stack: (v -- n)

**Signature:** `(v:Vec<T>) capacity<T>( -- n:i64)`
---

### `fn` dequeue

Remove and return the front element.  Returns error if queue is empty. Returns the value and updated queue.  Example:     q dequeue! -> q -> val  Stack: (q -- val q2)

**Signature:** `(q:Queue<T>) dequeue<T>( -- val:T q2:Queue<T>)!`
---

### `fn` enqueue

Add an element to the back of the queue.  Returns the updated queue which must be captured.  Example:     q 42 enqueue! -> q  Stack: (q val -- q2)

**Signature:** `(q:Queue<T>) enqueue<T>(val:T -- q2:Queue<T>)!`
---

### `fn` first

Get the first element of a pair.  Example:     p first -> a  Stack: (p -- a)

**Signature:** `(p:Pair<T, U>) first<T, U>( -- a:T)`
---

### `fn` get

Get the value for a key.  Returns an error if the key is not found.  Example:     m "alice" get! -> age  Stack: (m key -- value)

**Signature:** `(m:Map<V>) get<V>(key:str -- value:V)!`
---

### `fn` get

Get element at index.  Panics if index is out of bounds (< 0 or >= len).  Example:     v 0 get! -> first  Stack: (v idx -- elem)

**Signature:** `(v:Vec<T>) get<T>(idx:i64 -- elem:T)!`
---

### `fn` has

Check if a key exists in the map.  Example:     m "alice" has if { "found!" print nl }  Stack: (m key -- exists)

**Signature:** `(m:Map<V>) has<V>(key:str -- exists:i64)`
---

### `fn` insert

Insert a key-value pair into the map.  If the key already exists, the value is updated. Returns the updated map which must be captured.  Example:     m "alice" 42 insert! -> m  Stack: (m key value -- m2)

**Signature:** `(m:Map<V>) insert<V>(key:str value:V -- m2:Map<V>)!`
---

### `fn` is_empty

Check if the deque is empty.  Stack: (d -- bool)

**Signature:** `(d:Deque<T>) is_empty<T>( -- b:i64)`
---

### `fn` is_empty

Check if the map is empty.  Example:     m is_empty if { "empty!" print nl }  Stack: (m -- bool)

**Signature:** `(m:Map<V>) is_empty<V>( -- b:i64)`
---

### `fn` is_empty

Check if the queue is empty.  Stack: (q -- bool)

**Signature:** `(q:Queue<T>) is_empty<T>( -- b:i64)`
---

### `fn` is_empty

Check if the vector is empty (length == 0).  Example:     v is_empty if { "empty!" print nl }  Stack: (v -- bool)

**Signature:** `(v:Vec<T>) is_empty<T>( -- b:i64)`
---

### `fn` length

Get the number of elements.  Stack: (d -- n)

**Signature:** `(d:Deque<T>) length<T>( -- n:i64)`
---

### `fn` length

Get the number of entries in the map.  Example:     m length -> n  Stack: (m -- n)

**Signature:** `(m:Map<V>) length<V>( -- n:i64)`
---

### `fn` length

Get the number of elements in the queue.  Stack: (q -- n)

**Signature:** `(q:Queue<T>) length<T>( -- n:i64)`
---

### `fn` length

Get the number of elements in the vector.  Example:     v length -> n  Stack: (v -- n)

**Signature:** `(v:Vec<T>) length<T>( -- n:i64)`
---

### `fn` peek_back

View the back element without removing it.  Stack: (d -- val)

**Signature:** `(d:Deque<T>) peek_back<T>( -- val:T)!`
---

### `fn` peek_front

View the front element without removing it.  Stack: (d -- val)

**Signature:** `(d:Deque<T>) peek_front<T>( -- val:T)!`
---

### `fn` peek

View the front element without removing it.  Returns error if queue is empty.  Example:     q peek! -> val  Stack: (q -- val)

**Signature:** `(q:Queue<T>) peek<T>( -- val:T)!`
---

### `fn` pop_back

Remove and return the back element.  Example:     d pop_back! -> d -> val  Stack: (d -- val d2)

**Signature:** `(d:Deque<T>) pop_back<T>( -- val:T d2:Deque<T>)!`
---

### `fn` pop_front

Remove and return the front element.  Example:     d pop_front! -> d -> val  Stack: (d -- val d2)

**Signature:** `(d:Deque<T>) pop_front<T>( -- val:T d2:Deque<T>)!`
---

### `fn` pop

Pop and return the last element.  Panics if the vector is empty. Returns both the element and the updated vector.  Example:     v pop! -> v -> elem  Stack: (v -- elem v2)

**Signature:** `(v:Vec<T>) pop<T>( -- elem:T v2:Vec<T>)!`
---

### `fn` push_back

Push an element to the back.  Example:     d 42 push_back! -> d  Stack: (d val -- d2)

**Signature:** `(d:Deque<T>) push_back<T>(val:T -- d2:Deque<T>)!`
---

### `fn` push_front

Push an element to the front.  Example:     d 42 push_front! -> d  Stack: (d val -- d2)

**Signature:** `(d:Deque<T>) push_front<T>(val:T -- d2:Deque<T>)!`
---

### `fn` push

Push an element to the end of the vector.  Automatically grows capacity when full (doubles size, starting at 8). Returns the updated vector which must be captured.  Example:     v 42 push! -> v  Stack: (v elem -- v2)

**Signature:** `(v:Vec<T>) push<T>(elem:T -- v2:Vec<T>)!`
---

### `fn` release

Free the deque's memory.  Stack: (d --)

**Signature:** `(d:Deque<T>) release<T>()`
---

### `fn` release

Free the map's memory.  IMPORTANT: Always call this when done with a map to prevent memory leaks. After calling release, the map should not be used.  Example:     m release  Stack: (m --)

**Signature:** `(m:Map<V>) release<V>()`
---

### `fn` release

Free the queue's memory.  Stack: (q --)

**Signature:** `(q:Queue<T>) release<T>()`
---

### `fn` release

Free the vector's memory.  IMPORTANT: Always call this when done with a vector to prevent memory leaks. After calling release, the vector should not be used.  Example:     v release  Stack: (v --)

**Signature:** `(v:Vec<T>) release<T>()`
---

### `fn` remove

Remove a key from the map.  Returns error if the key is not found. Returns the updated map which must be captured.  Example:     m "alice" remove! -> m  Stack: (m key -- m2)

**Signature:** `(m:Map<V>) remove<V>(key:str -- m2:Map<V>)!`
---

### `fn` reset

Remove all elements, keeping allocated capacity.  Useful for reusing a vector without reallocating.  Example:     v reset -> v  Stack: (v -- v2)

**Signature:** `(v:Vec<T>) reset<T>( -- v2:Vec<T>)`
---

### `fn` second

Get the second element of a pair.  Example:     p second -> b  Stack: (p -- b)

**Signature:** `(p:Pair<T, U>) second<T, U>( -- b:U)`
---

### `fn` set

Set element at index.  Panics if index is out of bounds (< 0 or >= len). Returns the updated vector which must be captured.  Example:     v 0 100 set! -> v  Stack: (v idx elem -- v2)

**Signature:** `(v:Vec<T>) set<T>(idx:i64 elem:T -- v2:Vec<T>)!`
---

### `fn` unpack

Unpack a pair onto the stack.  Returns both elements: first is deeper, second on top.  Example:     p unpack -> first_val -> second_val  Stack: (p -- a b)

**Signature:** `(p:Pair<T, U>) unpack<T, U>( -- a:T b:U)`
## Deque

Double-ended queue with elements of type T.  Fields:   data - Pointer to circular buffer   head - Index of front element   tail - Index after last element   len  - Number of elements   cap  - Buffer capacity

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `data` | `ptr` |  |
| `head` | `i64` |  |
| `tail` | `i64` |  |
| `len` | `i64` |  |
| `cap` | `i64` |  |

## Map

Hash map with string keys and values of type V.  Fields:   keys   - Array of string key pointers   values - Array of V values   states - Array of slot states (0=empty, 1=occupied, 2=deleted)   len    - Number of entries   cap    - Allocated capacity

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `keys` | `ptr` |  |
| `values` | `ptr` |  |
| `states` | `ptr` |  |
| `len` | `i64` |  |
| `cap` | `i64` |  |

## Pair

Pair<T, U> - A Generic Two-Element Tuple ========================================  Pair holds two values of potentially different types. Useful for returning multiple values or key-value associations.  ## Creating a Pair      Pair<i64, str> { first = 42 second = "hello" } -> p  ## Accessing Elements      p <<first -> key     p <<second -> value  ## Unpacking Both Elements      p unpack -> a -> b A pair of two values.  Fields:   first  - The first element of type T   second - The second element of type U

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `first` | `T` |  |
| `second` | `U` |  |

## Queue

FIFO Queue with elements of type T.  Fields:   data - Pointer to circular buffer   head - Index of front element   tail - Index after last element   len  - Number of elements   cap  - Buffer capacity

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `data` | `ptr` |  |
| `head` | `i64` |  |
| `tail` | `i64` |  |
| `len` | `i64` |  |
| `cap` | `i64` |  |

## Set

Set of unique string elements.  Fields:   keys   - Array of string key pointers   states - Array of slot states (0=empty, 1=occupied, 2=deleted)   len    - Number of elements   cap    - Allocated capacity

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `keys` | `ptr` |  |
| `states` | `ptr` |  |
| `len` | `i64` |  |
| `cap` | `i64` |  |

### Methods

#### `fn` add

Add an element to the set.  If the element already exists, the set is unchanged. Returns the updated set which must be captured.  Example:     s "alice" add! -> s  Stack: (s key -- s2)

**Signature:** `(s:Set) add(key:str -- s2:Set)!`
---

#### `fn` contains

Check if an element exists in the set.  Example:     s "alice" contains if { "found!" print nl }  Stack: (s key -- exists)

**Signature:** `(s:Set) contains(key:str -- exists:i64)`
---

#### `fn` is_empty

Check if the set is empty.  Example:     s is_empty if { "empty!" print nl }  Stack: (s -- bool)

**Signature:** `(s:Set) is_empty( -- b:i64)`
---

#### `fn` length

Get the number of elements in the set.  Example:     s length -> n  Stack: (s -- n)

**Signature:** `(s:Set) length( -- n:i64)`
---

#### `fn` release

Free the set's memory.  IMPORTANT: Always call this when done with a set to prevent memory leaks.  Example:     s release  Stack: (s --)

**Signature:** `(s:Set) release()`
---

#### `fn` remove

Remove an element from the set.  Returns error if the element is not found. Returns the updated set which must be captured.  Example:     s "alice" remove! -> s  Stack: (s key -- s2)

**Signature:** `(s:Set) remove(key:str -- s2:Set)!`

## Vec

Dynamic array (vector) structure.  Fields:   data - Pointer to contiguous element storage   len  - Current number of elements (0 to cap)   cap  - Allocated capacity in elements

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `data` | `ptr` |  |
| `len` | `i64` |  |
| `cap` | `i64` |  |

