# Quadrate Language Specification

**Version 2.0.0-alpha**

<small>SPDX-License-Identifier: CC0-1.0 — This specification is released into the public domain. Anyone may implement the Quadrate language without royalty or restriction.</small>

This document provides a complete specification of the Quadrate programming language, intended for implementers who wish to create their own Quadrate compiler or interpreter.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Lexical Structure](#2-lexical-structure)
3. [Type System](#3-type-system)
4. [Stack Machine Model](#4-stack-machine-model)
5. [Syntax and Grammar](#5-syntax-and-grammar)
6. [Control Flow](#6-control-flow)
7. [Functions](#7-functions)
8. [Structs](#8-structs)
9. [Module System](#9-module-system)
10. [Error Handling](#10-error-handling)
11. [Memory Management](#11-memory-management)
12. [Built-in Instructions](#12-built-in-instructions)
13. [Standard Library Interface](#13-standard-library-interface)
14. [Runtime Requirements](#14-runtime-requirements)
15. [Appendix A: Grammar Summary](#appendix-a-grammar-summary)
16. [Appendix B: Operator Reference](#appendix-b-operator-reference)

---

## 1. Introduction

Quadrate is a **stack-based programming language** that compiles to native code via LLVM. It combines the simplicity of stack-based execution with modern features like generics, closures, and a module system.

### 1.1 Design Principles

- **Stack-based execution**: All operations manipulate an explicit runtime stack
- **Postfix notation**: Operators follow their operands
- **Strong typing**: Types are checked at compile time with runtime validation
- **Reference counting**: Automatic memory management for heap objects
- **Module system**: Code organization with public/private visibility

### 1.2 Terminology

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED", "MAY", and "OPTIONAL" in this document are to be interpreted as described in [RFC 2119](https://datatracker.ietf.org/doc/html/rfc2119).

### 1.3 Execution Model

A Quadrate program MUST consist of:
1. Module imports (`use` statements)
2. Constant declarations
3. Struct declarations
4. Function declarations
5. A `main` function (entry point)

Execution MUST begin at `main`, which operates on a shared runtime stack.

---

## 2. Lexical Structure

### 2.1 Character Set

Quadrate source files MUST be UTF-8 encoded. Identifiers MUST contain only ASCII letters, digits, and underscores.

### 2.2 Comments

```
// Line comment - extends to end of line

/* Block comment
   Can span multiple lines
   /* Can be nested */
*/
```

### 2.3 Tokens

#### 2.3.1 Keywords

```
fn        pub       struct    enum      use       import
if        else      for       while     loop
switch    break     continue  return    defer
const     test      ctx       as
```

#### 2.3.2 Predefined Constants

These identifiers are reserved and MUST evaluate to integer literals:

| Constant | Value | Description |
|----------|-------|-------------|
| `true` | `1` | Boolean true |
| `false` | `0` | Boolean false |
| `Ok` | `1` | Success result |
| `Err` | `0` | Error result |

**Note**: `true`/`Ok` and `false`/`Err` are interchangeable pairs.

#### 2.3.3 Identifiers

```
identifier := letter (letter | digit | '_')*
letter     := 'a'..'z' | 'A'..'Z'
digit      := '0'..'9'
```

Naming conventions (RECOMMENDED):
- **Structs**: `PascalCase` (e.g., `Point`, `HttpRequest`)
- **Constants**: `PascalCase` (e.g., `MaxSize`, `DefaultTimeout`)
- **Functions**: `snake_case` (e.g., `get_value`, `parse_input`)
- **Variables**: `snake_case` (e.g., `file_path`, `line_count`)
- **Type parameters**: Single uppercase letters (`T`, `U`, `V`)

#### 2.3.4 Literals

**Integer literals:**
```
integer := digit+
         | '0x' hex_digit+
         | '0b' bin_digit+
```
Examples: `42`, `0`, `1000`, `0xFF`, `0b1010`

**Float literals:**
```
float := digit+ '.' digit+
```
Examples: `3.14`, `0.0`, `100.5`

**String literals:**
```
string := '"' character* '"'
```
Escape sequences: `\n`, `\r`, `\t`, `\\`, `\"`

Examples: `"hello"`, `"line1\nline2"`

#### 2.3.5 Operators and Punctuation

**Arithmetic operators:**
```
+     Addition
-     Subtraction
*     Multiplication
/     Division
%     Modulo
```

**Comparison operators:**
```
<     Less than
>     Greater than
==    Equal
!=    Not equal
<=    Less than or equal
>=    Greater than or equal
```

**Other operators:**
```
++    Increment
--    Decrement
<<    Shift left
>>    Shift right
->    Local variable binding
::    Scope resolution
@     Field access (read)
.     Field set (write)
&     Function pointer prefix
!     Abort-on-error suffix
```

**Punctuation:**
```
:     Type annotation
,     Type parameter separator
( )   Function signatures
{ }   Blocks
[ ]   Array literals
```

**Note**: Bitwise operators `and`, `or`, `xor`, `not` MUST use named forms (no symbolic equivalents). Shift operators support both symbolic (`<<`, `>>`) and named (`shl`, `shr`) forms.

### 2.4 Whitespace

Whitespace (spaces, tabs, newlines) separates tokens but is otherwise insignificant. Indentation MUST NOT be syntactically meaningful.

---

## 3. Type System

### 3.1 Primitive Types

Implementations MUST support these four primitive types:

| Type  | Description | Size |
|-------|-------------|------|
| `i64` | Signed 64-bit integer | 8 bytes |
| `f64` | IEEE 754 double-precision float | 8 bytes |
| `str` | Immutable UTF-8 string | pointer |
| `ptr` | Generic pointer | 8 bytes |

**Note**: Quadrate MUST use only 64-bit integers and floats. There MUST NOT be 8/16/32-bit types in the language itself.

### 3.2 Composite Types

#### 3.2.1 Structs

User-defined composite types with named fields:

```quadrate
struct Point {
    x:f64
    y:f64
}

struct Person {
    name:str
    age:i64
}
```

Structs MUST be represented as `ptr` on the stack (pointer to heap or stack allocated data).

#### 3.2.2 Arrays

Dynamic arrays are created via array literals or `make<T>` instruction:

```quadrate
[1 2 3 4 5]           // Integer array
[1.0 2.0 3.0]         // Float array
["a" "b" "c"]         // String array
[[1 2] [3 4]]         // Nested array
```

Arrays MUST be represented as `ptr` on the stack.

### 3.3 Generic Types

Functions and structs can be parameterized over types:

```quadrate
fn identity<T>(x:T -- y:T) {
    -> val
    val
}

struct Box<T> {
    value:T
}

struct Pair<A, B> {
    first:A
    second:B
}
```

Type parameters SHOULD be single uppercase letters or short uppercase names.

### 3.4 Function Types

Function pointers MUST be represented as `ptr`:

```quadrate
fn apply(x:i64 f:ptr -- result:i64) {
    -> func -> val
    val func call
}

// Get function pointer
&double -> func_ptr
```

### 3.5 Special Types

| Type | Description |
|------|-------------|
| `any` | Wildcard type for operations accepting any type |
| `UNKNOWN` | Internal: unresolved type during compilation |
| `TAINTED` | Internal: error-tainted value from fallible function |
| `TYPEVAR` | Internal: type variable in generic context |

### 3.6 Type Coercion

Explicit casting via `cast<Type>`:

```quadrate
3.14 cast<i64>    // Float to int (truncates toward zero)
42 cast<f64>      // Int to float
100 cast<str>     // Any to string
"42" cast<i64>    // String to int (parse)
```

**Implicit coercion**: Implementations MUST NOT perform implicit type coercion. All type conversions MUST be explicit.

### 3.7 Type Narrowing

The `as` keyword narrows a `ptr` value to a specific struct type. This is a compile-time annotation with no runtime cost:

```quadrate
value as StructType @field    // access field on specific struct type
value as StructType -> local  // bind with tracked struct type
```

Implementations MUST produce an error when `@field` is used on an untyped `ptr` and the field name is ambiguous (exists in multiple struct definitions). The `as` keyword resolves this ambiguity.

---

## 4. Stack Machine Model

### 4.1 Runtime Stack

The Quadrate runtime MUST maintain a single data stack shared across all function calls.

**Stack element structure:**
```c
struct qd_stack_element {
    union {
        int64_t i;      // Integer value
        double f;       // Float value
        void* p;        // Pointer value
        qd_string_t* s; // String reference
    } value;

    qd_stack_type type;      // Type tag: INT=0, FLOAT=1, PTR=2, STR=3
    bool is_error_tainted;   // Error propagation flag
};
```

### 4.2 Stack Operations

All operations MUST be postfix. Arguments MUST be popped from the stack, and results MUST be pushed.

**Duplication:**

| Operation | Stack Effect | Description |
|-----------|--------------|-------------|
| `dup` | `( a -- a a )` | Duplicate top |
| `dup2` | `( a b -- a b a b )` | Duplicate top two |
| `dupd` | `( a b -- a a b )` | Duplicate second |

**Swapping:**

| Operation | Stack Effect | Description |
|-----------|--------------|-------------|
| `swap` | `( a b -- b a )` | Swap top two |
| `swap2` | `( a b c d -- c d a b )` | Swap pairs |
| `swapd` | `( a b c -- b a c )` | Swap under top |

**Removal:**

| Operation | Stack Effect | Description |
|-----------|--------------|-------------|
| `drop` | `( a -- )` | Remove top |
| `drop2` | `( a b -- )` | Remove top two |
| `nip` | `( a b -- b )` | Remove second |
| `nipd` | `( a b c -- a c )` | Remove second under top |
| `clear` | `( ... -- )` | Clear entire stack |

**Rearrangement:**

| Operation | Stack Effect | Description |
|-----------|--------------|-------------|
| `over` | `( a b -- a b a )` | Copy second to top |
| `over2` | `( a b c d -- a b c d a b )` | Copy second pair |
| `overd` | `( a b c -- a b a c )` | Copy third |
| `rot` | `( a b c -- b c a )` | Rotate three |
| `tuck` | `( a b -- b a b )` | Copy top below second |
| `pick` | `( ... n -- ... x )` | Copy nth to top |
| `roll` | `( ... n -- ... )` | Move nth to top |
| `nth` | `( array n -- elem )` | Get array element |
| `depth` | `( -- n )` | Push stack depth |

### 4.3 Stack Effect Signatures

Functions declare their stack effects:

```
fn name(input1:type1 input2:type2 -- output1:type1 output2:type2)
```

- **Left of `--`**: Parameters consumed from stack (bottom to top)
- **Right of `--`**: Values produced on stack (bottom to top)

Examples:
```quadrate
fn add(a:i64 b:i64 -- sum:i64)        // Consumes 2, produces 1
fn swap_pair(a:i64 b:i64 -- b:i64 a:i64)  // Consumes 2, produces 2
fn print_value(x:i64 -- )              // Consumes 1, produces 0
fn get_constant( -- c:i64)             // Consumes 0, produces 1
```

### 4.4 Local Variables

The `->` operator pops values into named local variables:

```quadrate
fn example(x:i64 y:i64 -- result:i64) {
    -> b        // Pop top into b
    -> a        // Pop next into a
    a b +       // Use variables
}
```

Variables MUST be stored in function-local allocas and MAY be referenced multiple times.

**Binding multiple values:**
```quadrate
10 20 30 -> z -> y -> x    // Each -> binds one value: z=30, y=20, x=10
```

**Note**: Each `->` MUST bind exactly one value. Multiple values require multiple `->` operators.

---

## 5. Syntax and Grammar

### 5.1 Program Structure

```
program := declaration*

declaration := use_statement
             | const_declaration
             | enum_declaration
             | struct_declaration
             | function_declaration
             | import_declaration
             | test_declaration
```

### 5.2 Use Statements

```quadrate
use module_name           // Import module by name
use "path/to/file.qd"     // Import file by path
use "./relative/path"     // Relative import
```

### 5.3 Constant Declarations

```quadrate
const NAME = literal
const PI = 3.14159
const MESSAGE = "hello"
const FROM_ENV = env("VAR_NAME")
const WITH_DEFAULT = env("VAR", "default")
```

### 5.4 Enum Declarations

```quadrate
enum Name {
    Variant1             // 0
    Variant2             // 1
    Variant3 = 10        // explicit value
    Variant4             // 11 (auto-increment from previous)
    Variant5 = -1        // negative values allowed
}
```

Enums define scoped named integer constants. Values are `i64` aliases — fully interchangeable with `i64` in all operations. Variants auto-increment from 0 or from the last explicit value + 1.

Access variants with `EnumName::Variant`. From modules: `module::EnumName::Variant`.

Use `pub enum` to export from a module.

### 5.5 Struct Declarations

```quadrate
[pub] struct Name [<TypeParams>] {
    field1:type1
    field2:type2 = default_value
    field3:*StructName           // Pointer type
}
```

### 5.6 Function Declarations

```quadrate
[pub] fn name [<TypeParams>] (params -- returns) [!] {
    body
}
```

- `pub`: Makes function publicly accessible from other modules
- `<TypeParams>`: Generic type parameters
- `!`: Marks function as fallible (can fail with error)

### 5.7 Import Declarations (FFI)

```quadrate
import "library.a" as "namespace" {
    [pub] fn funcname(params -- returns) [!]
    ...
}
```

Import blocks declare bindings to native C functions. Only function declarations are allowed inside the block. Constants associated with the library (such as error codes) should be declared at module top-level using `pub const`.

### 5.8 Test Declarations

```quadrate
test "test name" {
    // test body
}
```

---

## 6. Control Flow

### 6.1 If-Else

Condition MUST be popped from stack; non-zero MUST be treated as truthy:

```quadrate
condition if {
    // then branch
} else {
    // else branch (optional)
}
```

**Chained conditions:**
```quadrate
x 10 > if {
    "greater than 10"
} else {
    x 5 > if {
        "greater than 5"
    } else {
        "5 or less"
    }
}
```

### 6.2 For Loops

```quadrate
start end step for iterator {
    // body - iterator is available as local
}
```

**Stack**: MUST pop `step`, `end`, `start` from stack.

```quadrate
0 10 1 for i {
    i print nl
}

// Reverse iteration
10 0 -1 for i {
    i print nl
}
```

### 6.3 While Loops

```quadrate
condition while {
    // body
    condition  // last expression becomes condition for next iteration
}
```

The initial condition MUST be on the stack before `while`. The block MUST execute if the condition is true (non-zero). The block's last expression MUST become the condition for the next iteration.

**Example:**
```quadrate
0 -> i
i 5 < while {
    i print nl
    i 1 + -> i
    i 5 <        // condition for next iteration
}
```

### 6.4 Infinite Loops

```quadrate
loop {
    // body
    // must use 'break' to exit
}
```

### 6.5 Switch-Case

```quadrate
value switch {
    1 { "one" }
    2 { "two" }
    3 { "three" }
    _ { "other" }   // Default case (wildcard)
}
```

Cases can match:
- Integer literals
- String literals
- Constants (including scoped: `module::Constant`)
- Wildcard `_` for default

### 6.6 Break and Continue

```quadrate
break      // Exit innermost loop
continue   // Skip to next iteration
```

### 6.7 Defer

Deferred code MUST execute when scope exits in LIFO order:

```quadrate
fn process() {
    resource_open -> handle
    defer { handle resource_close }

    // ... work with handle ...
    // cleanup runs automatically
}
```

Multiple defers execute in reverse order:

```quadrate
fn example() {
    defer { "third" print nl }
    defer { "second" print nl }
    defer { "first" print nl }
}
// Prints: first, second, third
```

---

## 7. Functions

### 7.1 Function Definition

```quadrate
[pub] fn name [<T, U>] (input_params -- output_params) [!] {
    body
}
```

### 7.2 Function Calls

Functions are called by name; arguments must already be on stack:

```quadrate
10 20 add       // Call 'add' with 10 and 20 on stack
"hello" print   // Call 'print' with string on stack
```

**Module-qualified calls:**
```quadrate
3.14 math::sin      // Call 'sin' from 'math' module
"hello" strings::len    // Call 'len' from 'strings' module
```

### 7.3 Generic Functions

```quadrate
fn identity<T>(x:T -- y:T) {
    -> val
    val
}

fn pair<A, B>(a:A b:B -- first:A second:B) {
    -> b -> a
    a b
}
```

Type parameters are inferred from arguments at call sites.

### 7.4 Methods

Methods are functions with a receiver parameter in parentheses before the function name:

```quadrate
struct Vec2 { x:f64 y:f64 }

fn (v:Vec2) length( -- len:f64) {
    v @x v @x * v @y v @y * + math::sqrt
}

// Call as method
vec length
```

### 7.5 Anonymous Functions (Closures)

```quadrate
fn (params -- returns) { body }
```

**Implicit capture**: Variables from the enclosing scope MUST be automatically captured when referenced inside the closure body. An explicit capture list MUST NOT be required.

**Example:**
```quadrate
42 -> x
fn ( -- r:i64) { x } -> get_x    // x is automatically captured
get_x call print nl              // Prints 42
```

**Multiple captures:**
```quadrate
10 -> a
20 -> b
fn ( -- r:i64) { a b + } -> sum  // Both a and b are captured
sum call print nl                // Prints 30
```

**Closure without captures** (plain function pointer):
```quadrate
fn (x:i64 y:i64 -- r:i64) { -> b -> a a b + } -> add_fn
3 4 add_fn call print nl         // Prints 7
```

### 7.6 Function Pointers

```quadrate
&function_name      // Get function pointer
pointer call        // Invoke function via pointer
```

---

## 8. Structs

### 8.1 Struct Definition

```quadrate
[pub] struct Name [<TypeParams>] {
    field1:type1
    field2:type2 = default_value
}
```

### 8.2 Struct Construction

```quadrate
StructName {
    field1 = expr1
    field2 = expr2
}
```

**Generic struct construction:**
```quadrate
Box<i64> { value = 42 }
Pair<str, i64> { first = "key" second = 100 }
```

### 8.3 Field Access (Read)

Use `@` operator:

```quadrate
point @x        // Read field 'x' from point
point @x @y     // Chained access (if x is a struct)
```

When the struct type cannot be determined (e.g., the value is typed as `ptr`) and the field name exists in multiple struct definitions, implementations MUST produce an error. Use `as` to disambiguate:

```quadrate
ptr_val as MyStruct @field
```

### 8.4 Field Set (Write)

Use `.` operator:

```quadrate
value struct.field    // Set field to value
```

**Example:**
```quadrate
Point { x = 0.0 y = 0.0 } -> p
5.0 p.x      // Set p.x to 5.0
10.0 p.y     // Set p.y to 10.0
```

### 8.5 Generic Structs

```quadrate
struct Box<T> {
    value:T
}

struct Result<T, E> {
    value:T
    error:E
    is_ok:i64
}
```

---

## 9. Module System

### 9.1 Module Structure

A module is either:
- A single `.qd` file
- A directory containing `.qd` files (all files are loaded and merged into the module namespace)

Test files (`*_test.qd`) are excluded from module loading.

### 9.2 Module Resolution

When `use modulename` is encountered, search order:

1. Local path: `./modulename/*.qd` (all `.qd` files in directory)
2. Include paths (from `-I` flags)
3. Third-party packages: `~/.quadrate/modules/`
4. `$QUADRATE_PATH` environment variable
5. `$QUADRATE_ROOT` environment variable
6. Relative to executable: `../share/quadrate/`
7. `$HOME/quadrate/`

### 9.3 Public and Private

By default, declarations MUST be private to their module.

```quadrate
pub fn public_function() { }    // Accessible from other modules
fn private_function() { }       // Only accessible within module

pub struct PublicStruct { }
struct PrivateStruct { }

pub const PUBLIC_CONST = 1
const PRIVATE_CONST = 2
```

### 9.4 Scoped Access

Use `::` to access module members:

```quadrate
use math
use io

math::sin       // Function
math::Pi        // Constant
io::Read        // Constant
io::File        // Struct
```

### 9.5 FFI Imports

Import C functions from static libraries:

```quadrate
import "libfoo.a" as "foo" {
    pub fn bar(x:i64 -- y:i64)
    pub fn baz(s:str -- )!       // Fallible
}

// Usage
42 foo::bar
```

### 9.6 Multi-File Modules

```
mymodule/
├── api.qd          // Public API functions
├── helpers.qd      // Helper functions
└── types.qd        // Type definitions
```

All `.qd` files in the directory are automatically loaded and merged into the same module namespace. No file has special significance - they are all equal. No explicit imports between files in the same module are needed.

```quadrate
// api.qd
pub fn main_feature() {
    helper_function    // Can call functions from helpers.qd
}

// helpers.qd
fn helper_function() { }
```

---

## 10. Error Handling

### 10.1 Fallible Functions

Functions that can fail MUST be marked with `!`:

```quadrate
fn divide(a:i64 b:i64 -- result:i64)! {
    dup 0 == if {
        drop drop
        "division by zero" -1 panic
    }
    /
}
```

### 10.2 The `panic` Instruction

Signal an error from within a fallible function:

```quadrate
"error message" error_code panic
```

- **Stack effect**: `(msg:str code:i64 -- )`
- Sets runtime error state
- Only valid in fallible functions

**Alternative: Error literal syntax:**
```quadrate
error { code = 100 message = "must be non-negative" } panic
```

The `error { code = N message = "..." }` syntax creates an anonymous error value that can be used with `panic`.

### 10.3 Error Handling at Call Site

**Abort on error (`!` operator):**
```quadrate
10 0 divide!    // Aborts program if divide fails
```

**Propagate error (`?` operator):**
```quadrate
fn wrapper(a:i64 b:i64 -- result:i64)! {
    divide?     // Propagates error to caller if divide fails
    2 *
}
```

The `?` operator requires the enclosing function to be fallible. On error, the function returns immediately, preserving the callee's error state.

**Check with if-else:**
```quadrate
10 0 divide if {
    // Success - result on stack
    print nl
} else {
    // Failure - handle error
    drop
    "Division failed" print nl
}
```

**Check with switch:**
```quadrate
path io::Read io::open switch {
    Ok {
        -> handle
        // use handle
        handle io::close
    }
    io::ErrNotFound {
        drop
        "File not found" print nl
    }
    _ {
        drop
        "Unknown error" print nl
    }
}
```

### 10.4 Error Constants

By convention:
- `Ok` = 1 (success)
- `Err` = 0 (generic error)
- Module-specific errors start at 2

Example from `io` module:
```quadrate
pub const ErrNotFound = 2
pub const ErrPermission = 3
pub const ErrInvalidHandle = 4
```

### 10.5 The `err` Instruction

Retrieve error information after a failed call:

```quadrate
err    // Stack effect: ( -- msg:str code:i64 )
```

---

## 11. Memory Management

### 11.1 Allocation Strategy

**Stack allocation:**
- Local structs that don't escape their function
- Faster, no reference counting overhead
- Automatic cleanup when function returns

**Heap allocation:**
- Structs returned from functions
- Strings
- Arrays
- Captured closure variables

### 11.2 Reference Counting

Heap objects MUST use atomic reference counting:

```c
struct qd_refcounted {
    atomic_size_t refcount;  // Initially 1
    // ... object data ...
};
```

**Operations:**
- **Retain**: Increment refcount (when sharing)
- **Release**: Decrement refcount; free when reaches 0

### 11.3 String Memory

Strings MUST be reference-counted and immutable:

```c
struct qd_string {
    char* data;
    size_t length;
    size_t capacity;
    atomic_size_t refcount;
};
```

- Copying a string increments its refcount (shallow copy)
- String operations that modify create new strings
- In-place mutation only when refcount == 1

### 11.4 Struct Memory

Heap-allocated structs have a hidden header:

```c
struct qd_struct_header {
    size_t magic;           // Validation marker
    atomic_size_t refcount;
    void (*destructor)(void*);
};
```

**Destructors:**
- Automatically generated for structs with string/struct fields
- Called when refcount reaches 0
- Release nested references

### 11.5 Array Memory

```c
struct qd_array {
    size_t magic;
    atomic_size_t refcount;
    size_t length;
    size_t capacity;
    qd_array_type elemType;  // INT, FLOAT, STR, PTR
    union {
        int64_t* i;
        double* f;
        void** p;
    } data;
};
```

- Dynamic resizing with 2x growth factor
- Element-type-aware cleanup (strings released, nested arrays/structs released)

### 11.6 Closure Captures

Variables referenced inside a closure MUST be automatically detected and captured (implicit capture). Captured variables MUST be heap-allocated with reference counting:

```c
struct captured_var {
    int64_t refcount;
    qd_stack_element_t elem;
};
```

- Closure increments refcount of captured variables
- Variables freed when all closures using them are released

### 11.7 Defer for Cleanup

Use `defer` for explicit cleanup:

```quadrate
fn process(path:str -- )! {
    path io::Read io::open!
    -> file
    defer { file io::close }

    // ... process file ...
    // close runs automatically on scope exit
}
```

---

## 12. Built-in Instructions

### 12.1 Arithmetic

| Instruction | Alias | Stack Effect | Description |
|-------------|-------|--------------|-------------|
| `add` | `+` | `(a b -- c)` | Addition |
| `sub` | `-` | `(a b -- c)` | Subtraction |
| `mul` | `*` | `(a b -- c)` | Multiplication |
| `div` | `/` | `(a b -- c)` | Division |
| `mod` | `%` | `(a b -- c)` | Modulo |
| `neg` | | `(a -- b)` | Negation |
| `inc` | `++` | `(a -- b)` | Increment by 1 |
| `dec` | `--` | `(a -- b)` | Decrement by 1 |

### 12.2 Comparison

| Instruction | Alias | Stack Effect | Description |
|-------------|-------|--------------|-------------|
| `eq` | `==` | `(a b -- bool)` | Equal |
| `neq` | `!=` | `(a b -- bool)` | Not equal |
| `lt` | `<` | `(a b -- bool)` | Less than |
| `gt` | `>` | `(a b -- bool)` | Greater than |
| `lte` | `<=` | `(a b -- bool)` | Less than or equal |
| `gte` | `>=` | `(a b -- bool)` | Greater than or equal |
| `within` | | `(x lo hi -- bool)` | lo <= x <= hi |

### 12.3 Bitwise

| Instruction | Alias | Stack Effect | Description |
|-------------|-------|--------------|-------------|
| `and` | | `(a b -- c)` | Bitwise AND |
| `or` | | `(a b -- c)` | Bitwise OR |
| `xor` | | `(a b -- c)` | Bitwise XOR |
| `not` | | `(a -- b)` | Bitwise NOT |
| `shl` | `<<` | `(a n -- b)` | Shift left |
| `shr` | `>>` | `(a n -- b)` | Shift right |

### 12.4 Stack Manipulation

See [Section 4.2](#42-stack-operations) for complete list.

### 12.5 Array Operations

| Instruction | Stack Effect | Description |
|-------------|--------------|-------------|
| `make<T>` | `(n -- arr)` | Create typed array of size n |
| `append` | `(arr elem -- arr)` | Append element |
| `set` | `(arr idx elem -- )` | Set element at index |
| `nth` | `(arr idx -- elem)` | Get element at index |
| `len` | `(arr -- n)` | Get array length |

### 12.6 Type Operations

| Instruction | Stack Effect | Description |
|-------------|--------------|-------------|
| `cast<T>` | `(a -- b)` | Cast to type T |
| `sizeof` | `(T -- n)` | Size of type in bytes |

### 12.7 I/O Operations

| Instruction | Stack Effect | Description |
|-------------|--------------|-------------|
| `print` | `(x -- )` | Print value |
| `printv` | `(x -- )` | Print value with type info |
| `prints` | `( -- )` | Print entire stack contents (debug) |
| `printsv` | `( -- )` | Print entire stack with type info (debug) |
| `nl` | `( -- )` | Print newline |
| `read` | `( -- ... n)` | Read command-line arguments (pushes args with type inference, then count) |

### 12.8 Error Handling

| Instruction | Stack Effect | Description |
|-------------|--------------|-------------|
| `panic` | `(msg code -- )` | Signal error (fallible functions only) |
| `err` | `( -- msg code)` | Get last error info |

### 12.9 Function Calls

| Instruction | Stack Effect | Description |
|-------------|--------------|-------------|
| `call` | `(ptr -- ...)` | Call function via pointer |

---

## 13. Standard Library Interface

The Quadrate standard library provides these core modules:

### 13.1 math

Mathematical functions and constants.

**Constants:** `Pi`, `E`, `Tau`, `Phi`, `Sqrt2`, `Ln2`, etc.

**Functions:**
- Trigonometric: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`
- Hyperbolic: `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`
- Exponential: `exp`, `exp2`, `pow`, `sqrt`, `cbrt`, `log`, `log2`, `log10`
- Rounding: `ceil`, `floor`, `round`, `trunc`
- Utility: `abs`, `min`, `max`, `clamp`, `lerp`, `sign`

### 13.2 strings

String manipulation.

**Functions:**
- `len`, `concat`, `contains`, `starts_with`, `ends_with`
- `index_of`, `last_index_of`, `substring!`
- `upper`, `lower`, `trim`, `trim_left`, `trim_right`
- `split!`, `join!`, `replace!`, `repeat`, `reverse`
- `char_at!`, `from_char`, `compare`, `count`

### 13.3 io

File and stream I/O.

**Constants:** `Read`, `Write`, `Append`, `ReadWrite`, `SeekSet`, `SeekCur`, `SeekEnd`

**Error codes:** `ErrNotFound`, `ErrPermission`, `ErrRead`, `ErrWrite`, etc.

**Functions:**
- `open!`, `close`, `read!`, `write!`, `seek!`, `tell!`, `eof`
- `readline!`, `read_file!`, `write_file!`

### 13.4 fmt

Formatted output.

**Functions:** `printf`, `sprintf`

### 13.5 os

Operating system interface.

**Functions:**
- Process: `exit`, `system`, `getpid`, `exec!`, `popen!`
- Environment: `getenv`, `setenv`
- Filesystem: `exists`, `delete!`, `rename!`, `copy!`, `mkdir!`, `rmdir!`
- Directory: `list!`, `walk!`, `glob!`, `cwd!`

### 13.6 time

Time operations.

**Constants:** Duration units, weekday/month names

**Functions:**
- `unix`, `now`, `sleep`
- `year`, `month`, `day`, `hour`, `minute`, `second`, `weekday`
- `is_leap_year`, `days_in_month`, `add`, `sub`, `before`, `after`

### 13.7 thread

Threading primitives.

**Structs:** `Thread`, `Mutex`, `Channel`, `WaitGroup`

**Functions:**
- Thread: `spawn!`, `join!`, `detach!`, `is_alive`
- Mutex: `mutex_new!`, `lock!`, `unlock!`, `try_lock`
- Channel: `chan_new!`, `send!`, `recv!`, `close`
- WaitGroup: `wg_new!`, `add`, `done`, `wait!`

### 13.8 mem

Low-level memory.

**Functions:**
- `alloc!`, `realloc!`, `free`
- `set_byte`, `get_byte`, `set_i64`, `get_i64`
- `copy`, `zero`, `fill`

### 13.9 Additional Modules

- `sb` - StringBuilder for efficient string building
- `bytes` - Byte buffer operations
- `bits` - Bit manipulation utilities
- `rand` - Random number generation (xorshift64*)
- `path` - File path manipulation
- `strconv` - String/number conversion
- `unicode` - Unicode character constants
- `limits` - Numeric type limits
- `term` - Terminal control
- `signal` - Signal handling
- `flag` - Command-line flag parsing
- `testing` - Unit testing utilities

---

## 14. Runtime Requirements

### 14.1 Context Structure

The runtime MUST maintain an execution context:

```c
typedef struct {
    qd_stack* st;           // Data stack
    int64_t error_code;     // Current error (0 = none)
    char* error_msg;        // Error message
    int argc;               // Command-line arg count
    char** argv;            // Command-line args

    // Call stack for debugging
    const char* call_stack[256];
    const char* call_stack_files[256];
    size_t call_stack_lines[256];
    size_t call_stack_depth;
} qd_context;
```

### 14.2 Stack Implementation

```c
typedef struct {
    qd_stack_element_t* data;
    size_t capacity;
    size_t size;
} qd_stack;
```

### 14.3 Required Runtime Functions

Implementations MUST provide the following runtime functions:

**Context:**
- `qd_create_context(capacity)` - Create new context
- `qd_free_context(ctx)` - Destroy context

**Stack:**
- `qd_push_i(ctx, int64)` - Push integer
- `qd_push_f(ctx, float64)` - Push float
- `qd_push_s(ctx, string)` - Push string (copy)
- `qd_push_s_ref(ctx, string)` - Push string (reference)
- `qd_push_p(ctx, ptr)` - Push pointer
- `qd_stack_pop(stack, elem_ptr)` - Pop element

**Operations:**
- `qd_add`, `qd_sub`, `qd_mul`, `qd_div`, `qd_mod` - Arithmetic
- `qd_lt`, `qd_gt`, `qd_eq`, `qd_neq`, `qd_lte`, `qd_gte` - Comparison
- `qd_and`, `qd_or`, `qd_xor`, `qd_not`, `qd_shl`, `qd_shr` - Bitwise

**Memory:**
- `qd_struct_alloc(size, destructor)` - Allocate struct
- `qd_struct_retain(ptr)` - Increment refcount
- `qd_struct_release(ptr)` - Decrement refcount
- `qd_string_retain(str)` - Retain string
- `qd_string_release(str)` - Release string
- `qd_array_create(capacity, type)` - Create array
- `qd_array_release(arr)` - Release array

**Debug:**
- `qd_push_call(ctx, func, file, line)` - Push call frame
- `qd_pop_call(ctx)` - Pop call frame
- `qd_print_stack_trace(ctx)` - Print trace

---

## Appendix A: Grammar Summary

```ebnf
program         = { declaration } ;
declaration     = use_stmt | const_decl | struct_decl | fn_decl | import_decl | test_decl ;

use_stmt        = "use" ( identifier | string ) ;
const_decl      = "const" identifier "=" expression ;
struct_decl     = ["pub"] "struct" identifier [type_params] "{" { field_decl } "}" ;
fn_decl         = ["pub"] "fn" identifier [type_params] signature ["!"] block ;
import_decl     = "import" string "as" string "{" { import_fn } "}" ;
test_decl       = "test" string block ;

type_params     = "<" identifier { "," identifier } ">" ;
signature       = "(" [ param_list ] "--" [ param_list ] ")" ;
param_list      = param { param } ;
param           = identifier ":" type ;
field_decl      = identifier ":" type [ "=" expression ] ;
import_fn       = ["pub"] "fn" identifier signature ["!"] ;

block           = "{" { statement } "}" ;
statement       = expression | if_stmt | for_stmt | while_stmt | loop_stmt
                | switch_stmt | defer_stmt | "break" | "continue" ;

if_stmt         = expression "if" block [ "else" block ] ;
for_stmt        = expression expression expression "for" identifier block ;
while_stmt      = expression "while" block ;  /* block's last expr is next condition */
loop_stmt       = "loop" block ;
switch_stmt     = expression "switch" "{" { case_clause } "}" ;
case_clause     = ( literal | identifier | "_" ) block ;
defer_stmt      = "defer" ( block | statement ) ;

expression      = literal | identifier | scoped_id | struct_const | array_lit
                | field_access | field_set | local_bind | fn_call | anon_fn
                | error_lit | operator | instruction | as_cast ;
as_cast         = "as" type ;

error_lit       = "error" "{" "code" "=" expression "message" "=" expression "}" ;

literal         = integer | float | string | "true" | "false" | "Ok" | "Err" ;
scoped_id       = identifier "::" identifier ;
struct_const    = identifier [type_args] "{" { field_init } "}" ;
array_lit       = "[" { expression } "]" ;
field_access    = "@" identifier ;
field_set       = expression identifier "." identifier ;
local_bind      = "->" identifier ;
anon_fn         = "fn" signature block ;  /* captures are implicit */
fn_call         = identifier [ "!" ] ;

type            = "i64" | "f64" | "str" | "ptr" | identifier [type_args] | "*" type ;
type_args       = "<" type { "," type } ">" ;

operator        = "+" | "-" | "*" | "/" | "%" | "++" | "--"
                | "<" | ">" | "==" | "!=" | "<=" | ">="
                | "<<" | ">>" ;

instruction     = "dup" | "swap" | "drop" | "over" | "rot" | "nip" | "tuck"
                | "pick" | "roll" | "nth" | "len" | "append" | "set"
                | "make" "<" type ">" | "cast" "<" type ">"
                | "print" | "nl" | "read" | "call" | "panic" | "err" | ... ;
```

---

## Appendix B: Operator Reference

### B.1 Arithmetic Operators

| Operator | Named | Stack Effect | Description |
|----------|-------|--------------|-------------|
| `+` | `add` | `(a b -- c)` | a + b |
| `-` | `sub` | `(a b -- c)` | a - b |
| `*` | `mul` | `(a b -- c)` | a * b |
| `/` | `div` | `(a b -- c)` | a / b |
| `%` | `mod` | `(a b -- c)` | a mod b |
| `++` | `inc` | `(a -- b)` | a + 1 |
| `--` | `dec` | `(a -- b)` | a - 1 |

### B.2 Comparison Operators

| Operator | Named | Stack Effect | Result |
|----------|-------|--------------|--------|
| `==` | `eq` | `(a b -- bool)` | 1 if a == b, else 0 |
| `!=` | `neq` | `(a b -- bool)` | 1 if a != b, else 0 |
| `<` | `lt` | `(a b -- bool)` | 1 if a < b, else 0 |
| `>` | `gt` | `(a b -- bool)` | 1 if a > b, else 0 |
| `<=` | `lte` | `(a b -- bool)` | 1 if a <= b, else 0 |
| `>=` | `gte` | `(a b -- bool)` | 1 if a >= b, else 0 |

### B.3 Bitwise Operators

| Operator | Named | Stack Effect | Description |
|----------|-------|--------------|-------------|
| `<<` | `shl` | `(a n -- b)` | a << n |
| `>>` | `shr` | `(a n -- b)` | a >> n |
| | `and` | `(a b -- c)` | a & b |
| | `or` | `(a b -- c)` | a \| b |
| | `xor` | `(a b -- c)` | a ^ b |
| | `not` | `(a -- b)` | ~a |

### B.4 Special Operators

| Operator | Description |
|----------|-------------|
| `->` | Bind top of stack to local variable |
| `::` | Scope resolution (module::member) |
| `@` | Field access (read) |
| `.` | Field set (write) |
| `&` | Get function pointer |
| `!` | Abort on error (after fallible call) |
| `?` | Propagate error to caller (after fallible call) |
| `as` | Type narrowing cast (compile-time, no runtime cost) |

---

## Document History

- **2.0.0-alpha**: Initial specification document

---

*This specification is provided for implementers of Quadrate compilers and interpreters. For user documentation, see the [Quadrate Documentation](https://quad.r8.rs).*
