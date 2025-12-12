# Language Reference

This page documents all Quadrate keywords and built-in instructions.

## Keywords

| Keyword | Description |
|---------|-------------|
| [`fn`](#fn) | Declares a function with a stack signature. |
| [`pub`](#pub) | Makes a function, constant, or struct visible to other modules. |
| [`const`](#const) | Declares a compile-time constant value. |
| [`struct`](#struct) | Declares a structured data type with named fields. |
| [`use`](#use) | Imports a module, making its functions available with module::function syntax. |
| [`import`](#import) | Imports a C library, declaring its functions with Quadrate signatures. |
| [`if`](#if) | Executes a block if the top of stack is true (non-zero). |
| [`else`](#else) | Provides an alternative block when the if condition is false. |
| [`for`](#for) | Iterates from start to end with a step, binding the iterator variable. |
| [`loop`](#loop) | Repeats a block indefinitely until break is called. |
| [`while`](#while) | Repeats a block while the condition on the stack is true. |
| [`break`](#break) | Exits the innermost loop immediately. |
| [`continue`](#continue) | Skips to the next iteration of the innermost loop. |
| [`return`](#return) | Exits the current function immediately. |
| [`defer`](#defer) | Schedules a block to run when the function exits, in LIFO order. |
| [`switch`](#switch) | Branches based on matching the top of stack against case values. |
| [`=>`](#case-arrow) | Separates a case value from its block in a switch statement. |
| [`default`](#default) | Provides a fallback block when no switch case matches. |
| [`ctx`](#ctx) | Creates an isolated stack context; results are appended to parent stack. |
| [`->`](#arrow) | Pops a value from the stack and binds it to a local variable. |
| [`true`](#true) | Pushes 1 onto the stack. |
| [`false`](#false) | Pushes 0 onto the stack. |

### fn

Declares a function with a stack signature.

**Example:**

```qd
fn add(a:i64 b:i64 -- sum:i64) { + }
```

---

### pub

Makes a function, constant, or struct visible to other modules.

**Example:**

```qd
pub fn greet( -- ) { "Hello" print nl }
```

---

### const

Declares a compile-time constant value.

**Example:**

```qd
const PI = 3.14159
```

---

### struct

Declares a structured data type with named fields.

**Example:**

```qd
struct Point { x:f64 y:f64 }
```

---

### use

Imports a module, making its functions available with module::function syntax.

**Example:**

```qd
use str
```

---

### import

Imports a C library, declaring its functions with Quadrate signatures.

**Example:**

```qd
import "libmath.a" as "math" { pub fn sin(x:f64 -- y:f64) }
```

---

### if

Executes a block if the top of stack is true (non-zero).

**Example:**

```qd
5 3 > if { "yes" print }
```

---

### else

Provides an alternative block when the if condition is false.

**Example:**

```qd
x 0 > if { "positive" } else { "non-positive" }
```

---

### for

Iterates from start to end with a step, binding the iterator variable.

**Example:**

```qd
0 10 1 for i { i print nl }
```

---

### loop

Repeats a block indefinitely until break is called.

**Example:**

```qd
loop { "forever" print nl }
```

---

### while

Repeats a block while the condition on the stack is true.

**Example:**

```qd
1 -> x  x 10 < while { x print nl  x 1 + -> x  x 10 < }
```

---

### break

Exits the innermost loop immediately.

**Example:**

```qd
loop { x 10 > if { break } }
```

---

### continue

Skips to the next iteration of the innermost loop.

**Example:**

```qd
0 10 1 for i { i 5 == if { continue } i print nl }
```

---

### return

Exits the current function immediately.

**Example:**

```qd
fn early( -- ) { true if { return } "not reached" print }
```

---

### defer

Schedules a block to run when the function exits, in LIFO order.

**Example:**

```qd
defer { file io::close }
```

---

### switch

Branches based on matching the top of stack against case values.

**Example:**

```qd
x switch { 1 => { "one" } 2 => { "two" } default => { "other" } }
```

---

### =>

Separates a case value from its block in a switch statement.

**Example:**

```qd
1 => { "one" print }
```

---

### default

Provides a fallback block when no switch case matches.

**Example:**

```qd
default => { "no match" print }
```

---

### ctx

Creates an isolated stack context; results are appended to parent stack.

**Example:**

```qd
1 2 3 ctx { + + } // Stack: [1, 2, 3, 6]
```

---

### ->

Pops a value from the stack and binds it to a local variable.

**Example:**

```qd
42 -> x
```

---

### true

Pushes 1 onto the stack.

**Example:**

```qd
true if { "yes" print }
```

---

### false

Pushes 0 onto the stack.

**Example:**

```qd
false if { } else { "no" print }
```

---

## Built-in Instructions

### STACK OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`dup`](#dup) | `( a -- a a )` | Duplicates the top value on the stack. |
| [`dup2`](#dup2) | `( a b -- a b a b )` | Duplicates the top two values. |
| [`dupd`](#dupd) | `( a b -- a a b )` | Duplicates the second value, keeping top on top. |
| [`drop`](#drop) | `( a -- )` | Removes the top value from the stack. |
| [`drop2`](#drop2) | `( a b -- )` | Removes the top two values from the stack. |
| [`swap`](#swap) | `( a b -- b a )` | Exchanges the top two values. |
| [`swap2`](#swap2) | `( a b c d -- c d a b )` | Exchanges the top two pairs of values. |
| [`swapd`](#swapd) | `( a b c -- b a c )` | Swaps the second and third values, keeping top on top. |
| [`over`](#over) | `( a b -- a b a )` | Copies the second value to the top. |
| [`over2`](#over2) | `( a b c d -- a b c d a b )` | Copies the second pair to the top. |
| [`overd`](#overd) | `( a b c -- a b a c )` | Copies the second value, keeping top on top. |
| [`rot`](#rot) | `( a b c -- b c a )` | Rotates the top three values, moving third to top. |
| [`nip`](#nip) | `( a b -- b )` | Removes the second value, keeping top. |
| [`nipd`](#nipd) | `( a b c -- a c )` | Removes the second value, keeping top and third. |
| [`tuck`](#tuck) | `( a b -- b a b )` | Copies the top value under the second. |
| [`pick`](#pick) | `( ... n -- ... val )` | Copies the nth value (0-indexed from top) to the top. |
| [`roll`](#roll) | `( ... n -- ... )` | Moves the nth value to the top, shifting others down. |
| [`clear`](#clear) | `( ... -- )` | Removes all values from the stack. |
| [`depth`](#depth) | `( ... -- ... n )` | Pushes the number of values on the stack. |

#### dup

Duplicates the top value on the stack.

**Signature:** `( a -- a a )`

**Example:**

```qd
5 dup + // 10
```

---

#### dup2

Duplicates the top two values.

**Signature:** `( a b -- a b a b )`

**Example:**

```qd
1 2 dup2 // Stack: [1, 2, 1, 2]
```

---

#### dupd

Duplicates the second value, keeping top on top.

**Signature:** `( a b -- a a b )`

---

#### drop

Removes the top value from the stack.

**Signature:** `( a -- )`

**Example:**

```qd
1 2 3 drop // Stack: [1, 2]
```

---

#### drop2

Removes the top two values from the stack.

**Signature:** `( a b -- )`

---

#### swap

Exchanges the top two values.

**Signature:** `( a b -- b a )`

**Example:**

```qd
1 2 swap // Stack: [2, 1]
```

---

#### swap2

Exchanges the top two pairs of values.

**Signature:** `( a b c d -- c d a b )`

---

#### swapd

Swaps the second and third values, keeping top on top.

**Signature:** `( a b c -- b a c )`

---

#### over

Copies the second value to the top.

**Signature:** `( a b -- a b a )`

**Example:**

```qd
1 2 over // Stack: [1, 2, 1]
```

---

#### over2

Copies the second pair to the top.

**Signature:** `( a b c d -- a b c d a b )`

---

#### overd

Copies the second value, keeping top on top.

**Signature:** `( a b c -- a b a c )`

---

#### rot

Rotates the top three values, moving third to top.

**Signature:** `( a b c -- b c a )`

**Example:**

```qd
1 2 3 rot // Stack: [2, 3, 1]
```

---

#### nip

Removes the second value, keeping top.

**Signature:** `( a b -- b )`

**Example:**

```qd
1 2 nip // Stack: [2]
```

---

#### nipd

Removes the second value, keeping top and third.

**Signature:** `( a b c -- a c )`

---

#### tuck

Copies the top value under the second.

**Signature:** `( a b -- b a b )`

---

#### pick

Copies the nth value (0-indexed from top) to the top.

**Signature:** `( ... n -- ... val )`

**Example:**

```qd
1 2 3 4 2 pick // Copies index 2 (value 2) to top
```

---

#### roll

Moves the nth value to the top, shifting others down.

**Signature:** `( ... n -- ... )`

**Example:**

```qd
1 2 3 4 2 roll // Moves index 2 (value 2) to top, shifting others
```

---

#### clear

Removes all values from the stack.

**Signature:** `( ... -- )`

---

#### depth

Pushes the number of values on the stack.

**Signature:** `( ... -- ... n )`

**Example:**

```qd
1 2 3 depth // Stack: [1, 2, 3, 3]
```

---

### ARITHMETIC OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`+`](#plus) | `( a b -- sum )` | Adds two numbers. |
| [`add`](#add) | `( a b -- sum )` | Adds two numbers. |
| [`-`](#minus) | `( a b -- diff )` | Subtracts b from a. |
| [`sub`](#sub) | `( a b -- diff )` | Subtracts b from a. |
| [`*`](#star) | `( a b -- product )` | Multiplies two numbers. |
| [`mul`](#mul) | `( a b -- product )` | Multiplies two numbers. |
| [`/`](#slash) | `( a b -- quotient )` | Divides a by b. |
| [`div`](#div) | `( a b -- quotient )` | Divides a by b. |
| [`%`](#percent) | `( a b -- remainder )` | Computes a modulo b. |
| [`mod`](#mod) | `( a b -- remainder )` | Computes a modulo b. |
| [`neg`](#neg) | `( a -- -a )` | Negates a number. |
| [`inc`](#inc) | `( a -- a+1 )` | Adds 1 to a number. |
| [`dec`](#dec) | `( a -- a-1 )` | Subtracts 1 from a number. |

#### +

Adds two numbers.

**Signature:** `( a b -- sum )`

**Example:**

```qd
3 4 + // 7
```

---

#### add

Adds two numbers.

**Signature:** `( a b -- sum )`

---

#### -

Subtracts b from a.

**Signature:** `( a b -- diff )`

**Example:**

```qd
10 3 - // 7
```

---

#### sub

Subtracts b from a.

**Signature:** `( a b -- diff )`

---

#### *

Multiplies two numbers.

**Signature:** `( a b -- product )`

**Example:**

```qd
6 7 * // 42
```

---

#### mul

Multiplies two numbers.

**Signature:** `( a b -- product )`

---

#### /

Divides a by b.

**Signature:** `( a b -- quotient )`

**Example:**

```qd
20 4 / // 5
```

---

#### div

Divides a by b.

**Signature:** `( a b -- quotient )`

---

#### %

Computes a modulo b.

**Signature:** `( a b -- remainder )`

**Example:**

```qd
17 5 % // 2
```

---

#### mod

Computes a modulo b.

**Signature:** `( a b -- remainder )`

---

#### neg

Negates a number.

**Signature:** `( a -- -a )`

**Example:**

```qd
5 neg // -5
```

---

#### inc

Adds 1 to a number.

**Signature:** `( a -- a+1 )`

**Example:**

```qd
5 inc // 6
```

---

#### dec

Subtracts 1 from a number.

**Signature:** `( a -- a-1 )`

**Example:**

```qd
5 dec // 4
```

---

### COMPARISON OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`==`](#eqeq) | `( a b -- bool )` | Returns 1 if a equals b, 0 otherwise. |
| [`eq`](#eq) | `( a b -- bool )` | Returns 1 if a equals b, 0 otherwise. |
| [`!=`](#bangeq) | `( a b -- bool )` | Returns 1 if a does not equal b, 0 otherwise. |
| [`neq`](#neq) | `( a b -- bool )` | Returns 1 if a does not equal b, 0 otherwise. |
| [`<`](#lt) | `( a b -- bool )` | Returns 1 if a is less than b, 0 otherwise. |
| [`lt`](#lt) | `( a b -- bool )` | Returns 1 if a is less than b, 0 otherwise. |
| [`<=`](#lteq) | `( a b -- bool )` | Returns 1 if a is less than or equal to b, 0 otherwise. |
| [`lte`](#lte) | `( a b -- bool )` | Returns 1 if a is less than or equal to b, 0 otherwise. |
| [`>`](#gt) | `( a b -- bool )` | Returns 1 if a is greater than b, 0 otherwise. |
| [`gt`](#gt) | `( a b -- bool )` | Returns 1 if a is greater than b, 0 otherwise. |
| [`>=`](#gteq) | `( a b -- bool )` | Returns 1 if a is greater than or equal to b, 0 otherwise. |
| [`gte`](#gte) | `( a b -- bool )` | Returns 1 if a is greater than or equal to b, 0 otherwise. |
| [`within`](#within) | `( val low high -- bool )` | Returns 1 if val is in [low, high), 0 otherwise. |

#### ==

Returns 1 if a equals b, 0 otherwise.

**Signature:** `( a b -- bool )`

**Example:**

```qd
5 5 == // 1
```

---

#### eq

Returns 1 if a equals b, 0 otherwise.

**Signature:** `( a b -- bool )`

---

#### !=

Returns 1 if a does not equal b, 0 otherwise.

**Signature:** `( a b -- bool )`

**Example:**

```qd
5 3 != // 1
```

---

#### neq

Returns 1 if a does not equal b, 0 otherwise.

**Signature:** `( a b -- bool )`

---

#### <

Returns 1 if a is less than b, 0 otherwise.

**Signature:** `( a b -- bool )`

**Example:**

```qd
3 5 < // 1
```

---

#### lt

Returns 1 if a is less than b, 0 otherwise.

**Signature:** `( a b -- bool )`

---

#### <=

Returns 1 if a is less than or equal to b, 0 otherwise.

**Signature:** `( a b -- bool )`

**Example:**

```qd
5 5 <= // 1
```

---

#### lte

Returns 1 if a is less than or equal to b, 0 otherwise.

**Signature:** `( a b -- bool )`

---

#### >

Returns 1 if a is greater than b, 0 otherwise.

**Signature:** `( a b -- bool )`

**Example:**

```qd
5 3 > // 1
```

---

#### gt

Returns 1 if a is greater than b, 0 otherwise.

**Signature:** `( a b -- bool )`

---

#### >=

Returns 1 if a is greater than or equal to b, 0 otherwise.

**Signature:** `( a b -- bool )`

**Example:**

```qd
5 5 >= // 1
```

---

#### gte

Returns 1 if a is greater than or equal to b, 0 otherwise.

**Signature:** `( a b -- bool )`

---

#### within

Returns 1 if val is in [low, high), 0 otherwise.

**Signature:** `( val low high -- bool )`

**Example:**

```qd
5 0 10 within // 1
```

---

### BITWISE OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`and`](#and) | `( a b -- result )` | Computes bitwise AND of two integers. |
| [`or`](#or) | `( a b -- result )` | Computes bitwise OR of two integers. |
| [`xor`](#xor) | `( a b -- result )` | Computes bitwise XOR of two integers. |
| [`not`](#not) | `( a -- result )` | Computes bitwise NOT (ones' complement). |
| [`shl`](#shl) | `( a n -- result )` | Shifts a left by n bits. |
| [`shr`](#shr) | `( a n -- result )` | Shifts a right by n bits (arithmetic shift). |

#### and

Computes bitwise AND of two integers.

**Signature:** `( a b -- result )`

**Example:**

```qd
0b1100 0b1010 and // 0b1000
```

---

#### or

Computes bitwise OR of two integers.

**Signature:** `( a b -- result )`

**Example:**

```qd
0b1100 0b1010 or // 0b1110
```

---

#### xor

Computes bitwise XOR of two integers.

**Signature:** `( a b -- result )`

**Example:**

```qd
0b1100 0b1010 xor // 0b0110
```

---

#### not

Computes bitwise NOT (ones' complement).

**Signature:** `( a -- result )`

**Example:**

```qd
0 not // -1 (all bits set)
```

---

#### shl

Shifts a left by n bits.

**Signature:** `( a n -- result )`

**Example:**

```qd
1 4 shl // 16
```

---

#### shr

Shifts a right by n bits (arithmetic shift).

**Signature:** `( a n -- result )`

**Example:**

```qd
16 2 shr // 4
```

---

### ARRAY OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`makei`](#makei) | `( size -- arr )` | Creates an array of size integers, initialized to 0. |
| [`makef`](#makef) | `( size -- arr )` | Creates an array of size floats, initialized to 0.0. |
| [`makes`](#makes) | `( size -- arr )` | Creates an array of size strings, initialized to empty. |
| [`makep`](#makep) | `( size -- arr )` | Creates an array of size pointers, initialized to null. |
| [`make`](#make) | `( size -- arr )` | Creates a typed array (use with make<Type> syntax). |
| [`len`](#len) | `( arr -- len )` | Returns the number of elements in an array. |
| [`nth`](#nth) | `( arr index -- value )` | Returns the element at the given index. |
| [`set`](#set) | `( arr index value -- )` | Sets the element at the given index. |
| [`append`](#append) | `( arr value -- arr )` | Appends a value to the array, returning the modified array. |
| [`free`](#free) | `( arr -- )` | Frees the memory used by an array or struct. |

#### makei

Creates an array of size integers, initialized to 0.

**Signature:** `( size -- arr )`

**Example:**

```qd
10 makei -> arr
```

---

#### makef

Creates an array of size floats, initialized to 0.0.

**Signature:** `( size -- arr )`

**Example:**

```qd
10 makef -> arr
```

---

#### makes

Creates an array of size strings, initialized to empty.

**Signature:** `( size -- arr )`

**Example:**

```qd
10 makes -> arr
```

---

#### makep

Creates an array of size pointers, initialized to null.

**Signature:** `( size -- arr )`

**Example:**

```qd
10 makep -> arr
```

---

#### make

Creates a typed array (use with make<Type> syntax).

**Signature:** `( size -- arr )`

**Example:**

```qd
10 make<Point> -> points
```

---

#### len

Returns the number of elements in an array.

**Signature:** `( arr -- len )`

**Example:**

```qd
arr len // number of elements
```

---

#### nth

Returns the element at the given index.

**Signature:** `( arr index -- value )`

**Example:**

```qd
arr 0 nth // first element
```

---

#### set

Sets the element at the given index.

**Signature:** `( arr index value -- )`

**Example:**

```qd
arr 0 42 set
```

---

#### append

Appends a value to the array, returning the modified array.

**Signature:** `( arr value -- arr )`

**Example:**

```qd
arr 42 append -> arr
```

---

#### free

Frees the memory used by an array or struct.

**Signature:** `( arr -- )`

**Example:**

```qd
arr free
```

---

### TYPE CASTING

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`cast`](#cast) | `( val -- T )` | Converts a value to the specified type (use with cast<T> syntax). |

#### cast

Converts a value to the specified type (use with cast<T> syntax).

**Signature:** `( val -- T )`

**Example:**

```qd
3.14 cast<i64> // 3
42 cast<f64> // 42.0
42 cast<str> // "42"
```

---

### INPUT/OUTPUT

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`print`](#print) | `( val -- )` | Prints a value to stdout without a newline. |
| [`printv`](#printv) | `( val -- )` | Prints a value with type information for debugging. |
| [`prints`](#prints) | `( -- )` | Prints the entire stack contents without clearing it. |
| [`printsv`](#printsv) | `( -- )` | Prints the entire stack with type information for debugging. |
| [`nl`](#nl) | `( -- )` | Prints a newline character to stdout. |
| [`read`](#read) | `( -- ... n )` | Reads command line arguments onto the stack, pushing count last. |

#### print

Prints a value to stdout without a newline.

**Signature:** `( val -- )`

**Example:**

```qd
42 print
```

---

#### printv

Prints a value with type information for debugging.

**Signature:** `( val -- )`

---

#### prints

Prints the entire stack contents without clearing it.

**Signature:** `( -- )`

**Example:**

```qd
1 2 3 prints  // prints entire stack
```

---

#### printsv

Prints the entire stack with type information for debugging.

**Signature:** `( -- )`

---

#### nl

Prints a newline character to stdout.

**Signature:** `( -- )`

**Example:**

```qd
nl
```

---

#### read

Reads command line arguments onto the stack, pushing count last.

**Signature:** `( -- ... n )`

**Example:**

```qd
read -> argc // reads command line args
```

---

### ERROR HANDLING

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`error`](#error) | `( msg code -- )` | Signals an error with a message and code. Used in fallible functions. |
| [`err`](#err) | `( -- code )` | Pushes the error code from the last fallible function call. |
| [`panic`](#panic) | `( msg code -- )` | Signals an error and returns from the fallible function. |

#### error

Signals an error with a message and code. Used in fallible functions.

**Signature:** `( msg code -- )`

**Example:**

```qd
"invalid input" 1 error
```

---

#### err

Pushes the error code from the last fallible function call.

**Signature:** `( -- code )`

**Example:**

```qd
err print nl  // prints error code from last fallible call
```

---

#### panic

Signals an error and returns from the fallible function.

**Signature:** `( msg code -- )`

**Example:**

```qd
"invalid input" 1 panic
```

---

### THREADING

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`spawn`](#spawn) | `( fn -- thread )` | Spawns a new thread to execute a function. |
| [`wait`](#wait) | `( thread -- )` | Waits for a thread to complete. |
| [`detach`](#detach) | `( thread -- )` | Detaches a thread, allowing it to run independently. |

#### spawn

Spawns a new thread to execute a function.

**Signature:** `( fn -- thread )`

---

#### wait

Waits for a thread to complete.

**Signature:** `( thread -- )`

---

#### detach

Detaches a thread, allowing it to run independently.

**Signature:** `( thread -- )`

---

### MISCELLANEOUS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`call`](#call) | `( fn -- ... )` | Calls a function pointer obtained with &funcname syntax. |

#### call

Calls a function pointer obtained with &funcname syntax.

**Signature:** `( fn -- ... )`

**Example:**

```qd
&foo call  // calls the function foo
```
