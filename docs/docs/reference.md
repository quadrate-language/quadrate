# Language Reference

This page documents all Quadrate keywords and built-in instructions.

## Keywords

| Keyword | Description |
|---------|-------------|
| [`fn`](#fn) | Declares a function with a stack signature. Written without a name it is an anonymous function, which follows the same rules. |
| [`pub`](#pub) | Makes a function, constant, or struct visible to other modules. |
| [`const`](#const) | Declares a compile-time constant value. |
| [`var`](#var) | Declares a module-level mutable global (type annotation optional; inferred from initializer when omitted). |
| [`struct`](#struct) | Declares a structured data type with named fields. |
| [`packed`](#packed) | Struct modifier: fields laid out adjacent at exact widths, no padding. Used before `struct`. |
| [`stack`](#stack) | Function modifier: the inputs stay on the stack instead of being bound as locals, and any names they carry are documentation. Used before `fn`, named or anonymous. |
| [`use`](#use) | Imports a module, making its functions available with module::function syntax. |
| [`import`](#import) | Imports a C library, declaring its functions with Quadrate signatures. |
| [`if`](#if) | Executes a block if the top of stack is true (non-zero). |
| [`else`](#else) | Provides an alternative block when the if condition is false. |
| [`for`](#for) | Iterates from start to end with a step, binding the iterator variable. |
| [`loop`](#loop) | Repeats a block indefinitely until break is called. |
| [`while`](#while) | Repeats a block while the preceding condition holds; the condition is re-evaluated before every iteration. |
| [`break`](#break) | Exits the innermost loop immediately. |
| [`continue`](#continue) | Skips to the next iteration of the innermost loop. |
| [`return`](#return) | Exits the current function immediately. |
| [`defer`](#defer) | Schedules a block to run when the function exits, in LIFO order. |
| [`switch`](#switch) | Branches based on matching the top of stack against case values. |
| [`_`](#_) | Provides a fallback block when no switch case matches. |
| [`->`](#arrow) | Pops a value from the stack and binds it to a local variable. |
| [`true`](#true) | Pushes 1 onto the stack. |
| [`false`](#false) | Pushes 0 onto the stack. |

### fn

Declares a function with a stack signature. Written without a name it is an anonymous function, which follows the same rules.

**Example:**

```qd
fn add(a:i64 b:i64 -- sum:i64) { a b + }
```

---

### pub

Makes a function, constant, or struct visible to other modules.

**Example:**

```qd
pub fn greet() { "Hello" print nl }
```

---

### const

Declares a compile-time constant value.

**Example:**

```qd
const PI = 3.14159
```

---

### var

Declares a module-level mutable global (type annotation optional; inferred from initializer when omitted).

**Example:**

```qd
var counter = 0    // inferred i64
```

---

### struct

Declares a structured data type with named fields.

**Example:**

```qd
struct Point { x:f64 y:f64 }
```

---

### packed

Struct modifier: fields laid out adjacent at exact widths, no padding. Used before `struct`.

**Example:**

```qd
packed struct FileLump { offset:u32 size:u32 name:u64 }
```

---

### stack

Function modifier: the inputs stay on the stack instead of being bound as locals, and any names they carry are documentation. Used before `fn`, named or anonymous.

**Example:**

```qd
stack fn double(i64 -- r:i64) { 2 * }
```

---

### use

Imports a module, making its functions available with module::function syntax.

**Example:**

```qd
use strings
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

Repeats a block while the preceding condition holds; the condition is re-evaluated before every iteration.

**Example:**

```qd
0 -> i  i 5 < while { i print nl  i 1 + -> i }
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
fn early() { true if { return } "not reached" print }
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
x switch { 1 { "one" } 2 { "two" } _ { "other" } }
```

---

### _

Provides a fallback block when no switch case matches.

**Example:**

```qd
_ { "no match" print }
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

### short-circuiting forms.

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`and`](#and) | `( a b -- result )` | Logical AND: 1 if both values are non-zero, 0 otherwise. Both sides are always evaluated. `bits::and` is the bitwise one. |
| [`or`](#or) | `( a b -- result )` | Logical OR: 1 if either value is non-zero, 0 otherwise. Both sides are always evaluated. `bits::or` is the bitwise one. |
| [`not`](#not) | `( a -- result )` | Logical NOT: 1 if the value is zero, 0 otherwise. `bits::not` is the bitwise ones' complement. |

#### and

Logical AND: 1 if both values are non-zero, 0 otherwise. Both sides are always evaluated. `bits::and` is the bitwise one.

**Signature:** `( a b -- result )`

**Example:**

```qd
1 1 and // 1
2 1 and // 1 (any non-zero value is true
 bits::and would give 0)
```

---

#### or

Logical OR: 1 if either value is non-zero, 0 otherwise. Both sides are always evaluated. `bits::or` is the bitwise one.

**Signature:** `( a b -- result )`

**Example:**

```qd
0 0 or // 0
2 1 or // 1 (any non-zero value is true
 bits::or would give 3)
```

---

#### not

Logical NOT: 1 if the value is zero, 0 otherwise. `bits::not` is the bitwise ones' complement.

**Signature:** `( a -- result )`

**Example:**

```qd
0 not // 1
5 not // 0
```

---

### STRUCT OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`clone`](#clone) | `( s -- s2 )` | A new struct of the same type, fields copied. Shallow: what a field points to is shared. |

#### clone

A new struct of the same type, fields copied. Shallow: what a field points to is shared.

**Signature:** `( s -- s2 )`

**Example:**

```qd
p clone -> q
q 42 >>x drop // p is untouched
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

### TYPE CASTING

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`cast`](#cast) | `( val -- T )` | Converts a value between i64, f64, ptr and str (use with cast<T> syntax). |

#### cast

Converts a value between i64, f64, ptr and str (use with cast<T> syntax).

**Signature:** `( val -- T )`

**Example:**

```qd
3.14 cast<i64> // 3
42 cast<f64> // 42.0
42 cast<str> // "42"
```

---

### ARRAY OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`len`](#len) | `( arr -- len )` | Returns the number of elements in an array. |
| [`nth`](#nth) | `( arr index -- value )` | Returns the element at the given index. |
| [`set`](#set) | `( arr index value -- )` | Sets the element at the given index. |
| [`append`](#append) | `( arr value -- arr )` | Appends a value to the array, returning the modified array. |
| [`free`](#free) | `( arr -- )` | Frees the memory used by an array or struct. |

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

### INPUT/OUTPUT

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`print`](#print) | `( val -- )` | Prints a value to stdout without a newline. |
| [`printv`](#printv) | `( val -- )` | Prints a value with type information for debugging. |
| [`prints`](#prints) | `()` | Prints the entire stack contents without clearing it. |
| [`nl`](#nl) | `()` | Prints a newline character to stdout. |

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

**Signature:** `()`

**Example:**

```qd
1 2 3 prints  // prints entire stack
```

---

#### nl

Prints a newline character to stdout.

**Signature:** `()`

**Example:**

```qd
nl
```

---

### logical operations above. The shifts have no logical counterpart and stay here.

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`shl`](#shl) | `( a n -- result )` | Shifts a left by n bits. |
| [`shr`](#shr) | `( a n -- result )` | Shifts a right by n bits (arithmetic shift). |

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

### STACK OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`dup`](#dup) | `( a -- a a )` | Duplicates the top value on the stack. |
| [`dup2`](#dup2) | `( a b -- a b a b )` | Duplicates the top two values. |
| [`drop`](#drop) | `( a -- )` | Removes the top value from the stack. |
| [`swap`](#swap) | `( a b -- b a )` | Exchanges the top two values. |
| [`over`](#over) | `( a b -- a b a )` | Copies the second value to the top. |
| [`rot`](#rot) | `( a b c -- b c a )` | Rotates the top three values, moving third to top. |
| [`nip`](#nip) | `( a b -- b )` | Removes the second value, keeping top. |
| [`pick`](#pick) | `( x y z -- x y z x )` | Copies the third value from the top to the top. |
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

#### drop

Removes the top value from the stack.

**Signature:** `( a -- )`

**Example:**

```qd
1 2 3 drop // Stack: [1, 2]
```

---

#### swap

Exchanges the top two values.

**Signature:** `( a b -- b a )`

**Example:**

```qd
1 2 swap // Stack: [2, 1]
```

---

#### over

Copies the second value to the top.

**Signature:** `( a b -- a b a )`

**Example:**

```qd
1 2 over // Stack: [1, 2, 1]
```

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

#### pick

Copies the third value from the top to the top.

**Signature:** `( x y z -- x y z x )`

**Example:**

```qd
1 2 3 pick // 1 2 3 1
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

### ERROR HANDLING

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`err`](#err) | `( -- code )` | Pushes the error code from the last fallible function call. |
| [`panic`](#panic) | `( msg code -- )` | Signals an error and returns from the fallible function. |

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

