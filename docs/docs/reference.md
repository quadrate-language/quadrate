# Language Reference

This page documents all Quadrate keywords and built-in instructions.

## Keywords

| Keyword | Description |
|---------|-------------|
| [`fn`](#fn) | Declares a function with a stack signature. Its inputs stay on the stack for the body to work on; `def` declares one that binds them to their names instead. Written without a name it is an anonymous function, which follows the same rules. |
| [`pub`](#pub) | Makes a function, constant, or struct visible to other modules. |
| [`const`](#const) | Declares a compile-time constant value. |
| [`var`](#var) | Declares a module-level mutable global (type annotation optional; inferred from initializer when omitted). |
| [`struct`](#struct) | Declares a structured data type with named fields. |
| [`packed`](#packed) | Struct modifier: fields laid out adjacent at exact widths, no padding. Used before `struct`. |
| [`def`](#def) | Declares a function like `fn`, but each named input is bound as a local at entry instead of staying on the stack. Written without a name it is an anonymous function. |
| [`inline`](#inline) | Function modifier: every call is replaced by the function's body. Used before `fn` or `def`. |
| [`enum`](#enum) | Declares named integer constants, numbered from 0 and written Color::Red. |
| [`type`](#type) | Declares another name for a type, accepted wherever the type is. `bool` is a built-in alias for i64. |
| [`test`](#test) | Declares a named test block, run by quadc --test or quad test. |
| [`use`](#use) | Imports a module, making its functions available with module::function syntax. |
| [`import`](#import) | Imports a C library, declaring its functions with Quadrate signatures. |
| [`if`](#if) | Executes a block if the top of stack is true (non-zero). |
| [`else`](#else) | Provides an alternative block when the if condition is false. |
| [`for`](#for) | Iterates from start to end with a step, binding the iterator variable. |
| [`loop`](#loop) | Repeats a block indefinitely until break is called. |
| [`while`](#while) | Inside a loop: takes a flag and leaves the innermost loop when it is false (zero). |
| [`until`](#until) | Inside a loop: takes a flag and leaves the innermost loop when it is true (non-zero). |
| [`break`](#break) | Exits the innermost loop immediately. |
| [`continue`](#continue) | Skips to the next iteration of the innermost loop. |
| [`return`](#return) | Exits the current function immediately. |
| [`defer`](#defer) | Schedules a block to run when the function exits, in LIFO order. |
| [`switch`](#switch) | Branches based on matching the top of stack against case values. |
| [`_`](#_) | Provides a fallback block when no switch case matches. |
| [`->`](#arrow) | Pops a value from the stack and binds it to a local variable. |
| [`true`](#true) | Pushes 1 onto the stack. |
| [`false`](#false) | Pushes 0 onto the stack. |
| [`Ok`](#Ok) | Pushes 0, the status a fallible call leaves when it succeeded. |
| [`Err`](#Err) | Pushes 1, the generic error code a fallible call can panic with. |
| [`null`](#null) | Pushes the null pointer. |
| [`as`](#as) | Narrows a ptr to a struct type at compile time, so its fields can be read. Costs nothing at run time. |

### fn

Declares a function with a stack signature. Its inputs stay on the stack for the body to work on; `def` declares one that binds them to their names instead. Written without a name it is an anonymous function, which follows the same rules.

**Example:**

```qd
fn square(x:i64 -- r:i64) { dup * }
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

### def

Declares a function like `fn`, but each named input is bound as a local at entry instead of staying on the stack. Written without a name it is an anonymous function.

**Example:**

```qd
def area(w:f64 h:f64 -- a:f64) { w h * }
```

---

### inline

Function modifier: every call is replaced by the function's body. Used before `fn` or `def`.

**Example:**

```qd
inline fn sq(x:i64 -- y:i64) { dup * }
```

---

### enum

Declares named integer constants, numbered from 0 and written Color::Red.

**Example:**

```qd
enum Color { Red Green Blue }
```

---

### type

Declares another name for a type, accepted wherever the type is. `bool` is a built-in alias for i64.

**Example:**

```qd
type Score = i64
```

---

### test

Declares a named test block, run by quadc --test or quad test.

**Example:**

```qd
use testing
test "adds" { 1 2 + 3 testing::assert_eq }
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

Inside a loop: takes a flag and leaves the innermost loop when it is false (zero).

**Example:**

```qd
0 -> i  loop { i 5 < while  i print nl  i 1 + -> i }
```

---

### until

Inside a loop: takes a flag and leaves the innermost loop when it is true (non-zero).

**Example:**

```qd
loop { read_line -> line  line process  line "quit" == until }
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

### Ok

Pushes 0, the status a fallible call leaves when it succeeded.

**Example:**

```qd
Ok print  // 0
```

---

### Err

Pushes 1, the generic error code a fallible call can panic with.

**Example:**

```qd
Err print  // 1
```

---

### null

Pushes the null pointer.

**Example:**

```qd
null -> p
```

---

### as

Narrows a ptr to a struct type at compile time, so its fields can be read. Costs nothing at run time.

**Example:**

```qd
p as Point <<x
```

---

## Built-in Instructions

### short-circuiting forms.

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`and`](#and) | `( a b -- flag )` | Returns 1 if both values are non-zero, 0 otherwise; both are always evaluated. `bits::and` is the bitwise one. |
| [`or`](#or) | `( a b -- flag )` | Returns 1 if either value is non-zero, 0 otherwise; both are always evaluated. `bits::or` is the bitwise one. |
| [`not`](#not) | `( a -- flag )` | Returns 1 if the value is zero, 0 otherwise. `bits::not` is the bitwise ones' complement. |

#### and

Returns 1 if both values are non-zero, 0 otherwise; both are always evaluated. `bits::and` is the bitwise one.

**Signature:** `( a b -- flag )`

**Example:**

```qd
1 1 and // 1
2 1 and // 1 (any non-zero value is true
 bits::and would give 0)
```

---

#### or

Returns 1 if either value is non-zero, 0 otherwise; both are always evaluated. `bits::or` is the bitwise one.

**Signature:** `( a b -- flag )`

**Example:**

```qd
0 0 or // 0
2 1 or // 1 (any non-zero value is true
 bits::or would give 3)
```

---

#### not

Returns 1 if the value is zero, 0 otherwise. `bits::not` is the bitwise ones' complement.

**Signature:** `( a -- flag )`

**Example:**

```qd
0 not // 1
5 not // 0
```

---

### STRUCT OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`clone`](#clone) | `( x -- x2 )` | Returns a shallow copy of a struct or an array: what a field or an element refers to is shared. |

#### clone

Returns a shallow copy of a struct or an array: what a field or an element refers to is shared.

**Signature:** `( x -- x2 )`

**Example:**

```qd
p clone -> q
q 42 >>x drop // p is untouched
[1 2 3] clone -> b
```

---

### COMPARISON OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`==`](#eqeq) | `( a b -- flag )` | Returns 1 if a equals b, 0 otherwise. |
| [`!=`](#bangeq) | `( a b -- flag )` | Returns 1 if a does not equal b, 0 otherwise. |
| [`<`](#lt) | `( a b -- flag )` | Returns 1 if a is less than b, 0 otherwise. |
| [`<=`](#lteq) | `( a b -- flag )` | Returns 1 if a is less than or equal to b, 0 otherwise. |
| [`>`](#gt) | `( a b -- flag )` | Returns 1 if a is greater than b, 0 otherwise. |
| [`>=`](#gteq) | `( a b -- flag )` | Returns 1 if a is greater than or equal to b, 0 otherwise. |
| [`within`](#within) | `( a low high -- flag )` | Returns 1 if a is in [low, high], both ends included, 0 otherwise. |

#### ==

Returns 1 if a equals b, 0 otherwise.

**Signature:** `( a b -- flag )`

**Example:**

```qd
5 5 == // 1
```

---

#### !=

Returns 1 if a does not equal b, 0 otherwise.

**Signature:** `( a b -- flag )`

**Example:**

```qd
5 3 != // 1
```

---

#### <

Returns 1 if a is less than b, 0 otherwise.

**Signature:** `( a b -- flag )`

**Example:**

```qd
3 5 < // 1
```

---

#### <=

Returns 1 if a is less than or equal to b, 0 otherwise.

**Signature:** `( a b -- flag )`

**Example:**

```qd
5 5 <= // 1
```

---

#### >

Returns 1 if a is greater than b, 0 otherwise.

**Signature:** `( a b -- flag )`

**Example:**

```qd
5 3 > // 1
```

---

#### >=

Returns 1 if a is greater than or equal to b, 0 otherwise.

**Signature:** `( a b -- flag )`

**Example:**

```qd
5 5 >= // 1
```

---

#### within

Returns 1 if a is in [low, high], both ends included, 0 otherwise.

**Signature:** `( a low high -- flag )`

**Example:**

```qd
5 0 10 within // 1
```

---

### TYPE CASTING

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`cast`](#cast) | `( x -- T )` | Converts x to T, written cast<T>: one of i64, f64, d128, ptr and str. |
| [`sizeof`](#sizeof) | `( -- n )` | Returns the size of T in bytes, written sizeof<T>; known at compile time. |

#### cast

Converts x to T, written cast<T>: one of i64, f64, d128, ptr and str.

**Signature:** `( x -- T )`

**Example:**

```qd
3.14 cast<i64> // 3
42 cast<f64> // 42.0
42 cast<str> // "42"
42 cast<d128> // 42
```

---

#### sizeof

Returns the size of T in bytes, written sizeof<T>; known at compile time.

**Signature:** `( -- n )`

**Example:**

```qd
sizeof<i64> // 8
```

---

### ARRAY OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`len`](#len) | `( arr -- n )` | Returns the number of elements in an array, or of characters in a string. |
| [`nth`](#nth) | `( arr i -- elem )` | Returns the element at index i; in a string, the character there, as its codepoint. |
| [`set`](#set) | `( arr i x -- )` | Stores x at index i. |
| [`append`](#append) | `( arr x -- arr )` | Appends x and returns the array, which may have moved. |
| [`free`](#free) | `( p -- )` | Frees an array or a struct. |

#### len

Returns the number of elements in an array, or of characters in a string.

**Signature:** `( arr -- n )`

**Example:**

```qd
arr len // number of elements
"héllo" len // 5
```

---

#### nth

Returns the element at index i; in a string, the character there, as its codepoint.

**Signature:** `( arr i -- elem )`

**Example:**

```qd
arr 0 nth // first element
"abc" 1 nth // 98
```

---

#### set

Stores x at index i.

**Signature:** `( arr i x -- )`

**Example:**

```qd
arr 0 42 set
```

---

#### append

Appends x and returns the array, which may have moved.

**Signature:** `( arr x -- arr )`

**Example:**

```qd
arr 42 append -> arr
```

---

#### free

Frees an array or a struct.

**Signature:** `( p -- )`

**Example:**

```qd
arr free
```

---

### ARITHMETIC OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`+`](#plus) | `( a b -- sum )` | Adds two numbers. |
| [`-`](#minus) | `( a b -- diff )` | Subtracts b from a. |
| [`*`](#star) | `( a b -- product )` | Multiplies two numbers. |
| [`/`](#slash) | `( a b -- quotient )` | Divides a by b. |
| [`%`](#percent) | `( a b -- remainder )` | Computes a modulo b. |
| [`neg`](#neg) | `( a -- -a )` | Negates a number. |
| [`++`](#plusplus) | `( a -- a+1 )` | Adds 1 to a number. |
| [`--`](#minusminus) | `( a -- a-1 )` | Subtracts 1 from a number. |

#### +

Adds two numbers.

**Signature:** `( a b -- sum )`

**Example:**

```qd
3 4 + // 7
```

---

#### -

Subtracts b from a.

**Signature:** `( a b -- diff )`

**Example:**

```qd
10 3 - // 7
```

---

#### *

Multiplies two numbers.

**Signature:** `( a b -- product )`

**Example:**

```qd
6 7 * // 42
```

---

#### /

Divides a by b.

**Signature:** `( a b -- quotient )`

**Example:**

```qd
20 4 / // 5
```

---

#### %

Computes a modulo b.

**Signature:** `( a b -- remainder )`

**Example:**

```qd
17 5 % // 2
```

---

#### neg

Negates a number.

**Signature:** `( a -- -a )`

**Example:**

```qd
5 neg // -5
```

---

#### ++

Adds 1 to a number.

**Signature:** `( a -- a+1 )`

**Example:**

```qd
5 ++ // 6
```

---

#### --

Subtracts 1 from a number.

**Signature:** `( a -- a-1 )`

**Example:**

```qd
5 -- // 4
```

---

### one is checked on the real stack and compiled inline.

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`call`](#call) | `( q -- ... )` | Runs a function value: a fn { } literal, a local holding one, or &name. |
| [`dip`](#dip) | `( ..a x q -- ..b x )` | Runs q on what is under x, then puts x back. |
| [`keep`](#keep) | `( ..a x q -- ..b x )` | Runs q on x, then puts x back on top. |
| [`dip2`](#dip2) | `( ..a x y q -- ..b x y )` | Runs q on what is under the top two values, then puts them back. |
| [`keep2`](#keep2) | `( ..a x y q -- ..b x y )` | Runs q on the top two values, then puts them back on top. |
| [`bi`](#bi) | `( ..a x p q -- ..c )` | Runs p on x, then q on x again, each on what the last left. |
| [`tri`](#tri) | `( ..a x p q r -- ..d )` | Runs p, q and r on x in turn, as bi runs two. |
| [`both`](#both) | `( ..a x y q -- ..c )` | Runs q on x, then on y. |
| [`times`](#times) | `( ..a n q -- ..a )` | Runs q n times; it must leave the stack as it found it. |
| [`each`](#each) | `( ..a arr q -- ..a )` | Runs q with each element, or character of a string, on top; it must leave the stack as it found it. |
| [`map`](#map) | `( arr q -- arr2 )` | Returns a new array of what q leaves for each element. |
| [`filter`](#filter) | `( arr q -- arr2 )` | Returns a new array of the elements q leaves a true flag for. |
| [`fold`](#fold) | `( arr acc q -- acc )` | Runs q on the accumulator and each element, or character of a string, in turn, starting from acc. |
| [`any`](#any) | `( arr q -- flag )` | Returns 1 if q leaves a true flag for any element, or character of a string, stopping at the first that does. |
| [`all`](#all) | `( arr q -- flag )` | Returns 1 if q leaves a true flag for every element, or character of a string, stopping at the first that does not. |
| [`find`](#find) | `( arr q -- elem flag )` | Returns the first element, or character of a string, q leaves a true flag for, and 1; or the zero of the element type, and 0. |
| [`index`](#index) | `( arr q -- i )` | Returns the position of the first element, or character of a string, q leaves a true flag for, or -1. |
| [`scan`](#scan) | `( arr start q -- i )` | Returns the position of the first element, or character of a string, at or after start that q leaves a true flag for, or -1. |

#### call

Runs a function value: a fn { } literal, a local holding one, or &name.

**Signature:** `( q -- ... )`

**Example:**

```qd
&foo call  // calls the function foo
```

---

#### dip

Runs q on what is under x, then puts x back.

**Signature:** `( ..a x q -- ..b x )`

**Example:**

```qd
10 20 fn { 1 + } dip  // 11 20
```

---

#### keep

Runs q on x, then puts x back on top.

**Signature:** `( ..a x q -- ..b x )`

**Example:**

```qd
2 fn { dup * } keep  // 4 2
```

---

#### dip2

Runs q on what is under the top two values, then puts them back.

**Signature:** `( ..a x y q -- ..b x y )`

**Example:**

```qd
1 2 3 fn { 10 * } dip2  // 10 2 3
```

---

#### keep2

Runs q on the top two values, then puts them back on top.

**Signature:** `( ..a x y q -- ..b x y )`

**Example:**

```qd
2 3 fn { * } keep2  // 6 2 3
```

---

#### bi

Runs p on x, then q on x again, each on what the last left.

**Signature:** `( ..a x p q -- ..c )`

**Example:**

```qd
5 fn { 1 + } fn { 2 * } bi  // 6 10
```

---

#### tri

Runs p, q and r on x in turn, as bi runs two.

**Signature:** `( ..a x p q r -- ..d )`

**Example:**

```qd
5 fn { 1 + } fn { 2 * } fn { 3 - } tri  // 6 10 2
```

---

#### both

Runs q on x, then on y.

**Signature:** `( ..a x y q -- ..c )`

**Example:**

```qd
3 4 fn { 10 * } both  // 30 40
```

---

#### times

Runs q n times; it must leave the stack as it found it.

**Signature:** `( ..a n q -- ..a )`

**Example:**

```qd
1 3 fn { 2 * } times  // 8
```

---

#### each

Runs q with each element, or character of a string, on top; it must leave the stack as it found it.

**Signature:** `( ..a arr q -- ..a )`

**Example:**

```qd
0 [1 2 3] fn { + } each  // 6
```

---

#### map

Returns a new array of what q leaves for each element.

**Signature:** `( arr q -- arr2 )`

**Example:**

```qd
[1 2 3] fn { dup * } map  // [1 4 9]
```

---

#### filter

Returns a new array of the elements q leaves a true flag for.

**Signature:** `( arr q -- arr2 )`

**Example:**

```qd
[1 2 3 4] fn { 2 mod 0 == } filter  // [2 4]
```

---

#### fold

Runs q on the accumulator and each element, or character of a string, in turn, starting from acc.

**Signature:** `( arr acc q -- acc )`

**Example:**

```qd
[1 2 3] 0 fn { + } fold  // 6
```

---

#### any

Returns 1 if q leaves a true flag for any element, or character of a string, stopping at the first that does.

**Signature:** `( arr q -- flag )`

**Example:**

```qd
[1 5 12] fn { 10 > } any  // 1
"a1b" fn { 48 57 within } any  // 1
```

---

#### all

Returns 1 if q leaves a true flag for every element, or character of a string, stopping at the first that does not.

**Signature:** `( arr q -- flag )`

**Example:**

```qd
[1 5 12] fn { 0 > } all  // 1
```

---

#### find

Returns the first element, or character of a string, q leaves a true flag for, and 1; or the zero of the element type, and 0.

**Signature:** `( arr q -- elem flag )`

**Example:**

```qd
[1 5 12] fn { 4 > } find  // 5 1
```

---

#### index

Returns the position of the first element, or character of a string, q leaves a true flag for, or -1.

**Signature:** `( arr q -- i )`

**Example:**

```qd
[1 5 12] fn { 4 > } index  // 1
"key=value" fn { 61 == } index  // 3
```

---

#### scan

Returns the position of the first element, or character of a string, at or after start that q leaves a true flag for, or -1.

**Signature:** `( arr start q -- i )`

**Example:**

```qd
[3 8 5 12] 2 fn { 4 > } scan  // 2
"a=b=c" 2 fn { 61 == } scan  // 3
```

---

### INPUT/OUTPUT

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`print`](#print) | `( x -- )` | Prints a value without a newline. |
| [`dump`](#dump) | `( x -- )` | Prints a value with its type, for debugging. |
| [`nl`](#nl) | `( -- )` | Prints a newline. |

#### print

Prints a value without a newline.

**Signature:** `( x -- )`

**Example:**

```qd
42 print
```

---

#### dump

Prints a value with its type, for debugging.

**Signature:** `( x -- )`

---

#### nl

Prints a newline.

**Signature:** `( -- )`

**Example:**

```qd
nl
```

---

### STACK OPERATIONS

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`dup`](#dup) | `( x -- x x )` | Copies the top value. |
| [`dup2`](#dup2) | `( x y -- x y x y )` | Copies the top two values. |
| [`drop`](#drop) | `( x -- )` | Removes the top value. |
| [`swap`](#swap) | `( x y -- y x )` | Exchanges the top two values. |
| [`over`](#over) | `( x y -- x y x )` | Copies the second value to the top. |
| [`rot`](#rot) | `( x y z -- y z x )` | Moves the third value to the top. |
| [`rrot`](#rrot) | `( x y z -- z x y )` | Moves the top value below the next two; the inverse of rot. |
| [`tuck`](#tuck) | `( x y -- y x y )` | Copies the top value below the second. |
| [`drop2`](#drop2) | `( x y -- )` | Removes the top two values. |
| [`nip`](#nip) | `( x y -- y )` | Removes the second value. |
| [`pick`](#pick) | `( x y z -- x y z x )` | Copies the third value to the top. |

#### dup

Copies the top value.

**Signature:** `( x -- x x )`

**Example:**

```qd
5 dup + // 10
```

---

#### dup2

Copies the top two values.

**Signature:** `( x y -- x y x y )`

**Example:**

```qd
1 2 dup2 // Stack: [1, 2, 1, 2]
```

---

#### drop

Removes the top value.

**Signature:** `( x -- )`

**Example:**

```qd
1 2 3 drop // Stack: [1, 2]
```

---

#### swap

Exchanges the top two values.

**Signature:** `( x y -- y x )`

**Example:**

```qd
1 2 swap // Stack: [2, 1]
```

---

#### over

Copies the second value to the top.

**Signature:** `( x y -- x y x )`

**Example:**

```qd
1 2 over // Stack: [1, 2, 1]
```

---

#### rot

Moves the third value to the top.

**Signature:** `( x y z -- y z x )`

**Example:**

```qd
1 2 3 rot // Stack: [2, 3, 1]
```

---

#### rrot

Moves the top value below the next two; the inverse of rot.

**Signature:** `( x y z -- z x y )`

**Example:**

```qd
1 2 3 rrot  // Stack: 3 1 2
```

---

#### tuck

Copies the top value below the second.

**Signature:** `( x y -- y x y )`

**Example:**

```qd
1 2 tuck  // Stack: 2 1 2
```

---

#### drop2

Removes the top two values.

**Signature:** `( x y -- )`

**Example:**

```qd
1 2 3 drop2  // Stack: 1
```

---

#### nip

Removes the second value.

**Signature:** `( x y -- y )`

**Example:**

```qd
1 2 nip // Stack: [2]
```

---

#### pick

Copies the third value to the top.

**Signature:** `( x y z -- x y z x )`

**Example:**

```qd
1 2 3 pick // 1 2 3 1
```

---

### ERROR HANDLING

| Instruction | Signature | Description |
|-------------|-----------|-------------|
| [`err`](#err) | `( -- msg code )` | Returns the message and code of the last failed call, and clears them. |
| [`ok`](#ok) | `( status -- flag )` | Returns 1 if a fallible call's status is Ok (0), and 0 if it is an error code. |
| [`fail`](#fail) | `( status -- flag )` | Returns 1 if a fallible call's status is an error code, and 0 if it is Ok. |
| [`panic`](#panic) | `( msg code -- )` | Signals an error and returns from the fallible function. |

#### err

Returns the message and code of the last failed call, and clears them.

**Signature:** `( -- msg code )`

**Example:**

```qd
err print nl print nl  // the code, then the message, of the last failed call
```

---

#### ok

Returns 1 if a fallible call's status is Ok (0), and 0 if it is an error code.

**Signature:** `( status -- flag )`

**Example:**

```qd
10 2 divide ok if { print nl } else { drop }
```

---

#### fail

Returns 1 if a fallible call's status is an error code, and 0 if it is Ok.

**Signature:** `( status -- flag )`

**Example:**

```qd
path load fail if { drop "cannot load" print nl return }
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

