# Modules

Modules organize code into reusable units. They help structure larger programs.

## Importing Modules

Use `use` to import a module:

```qd
use io

fn main( -- ) {
	"Hello" io::println
}
```

The `use` statement makes the module available via its namespace.

## Calling Module Functions

Use `::` to call functions from a module:

```qd
use math
use str

fn main( -- ) {
	3.14159 math::sin print nl
	"hello" str::len print nl
}
```

## Multiple Imports

Import several modules at once:

```qd
use io
use math
use str
use os

fn main( -- ) {
	// Use functions from any imported module
	2.0 math::sqrt io::println
}
```

## Standard Library Modules

Quadrate includes many built-in modules:

| Module | Purpose |
|--------|---------|
| `io` | Input/output operations |
| `math` | Mathematical functions |
| `str` | String manipulation |
| `os` | Operating system interaction |
| `time` | Date and time |
| `mem` | Memory management |
| `net` | Networking |
| `bits` | Bit manipulation |
| `base64` | Base64 encoding |
| `hex` | Hexadecimal encoding |
| `path` | File path operations |
| `rand` | Random numbers |
| `json` | JSON parsing |
| `regex` | Regular expressions |
| `uuid` | UUID generation |

## Example: Using Multiple Modules

```qd
use io
use math
use time

fn main( -- ) {
	// Get current time
	time::now -> t

	// Do some math
	2.0 math::sqrt -> root

	// Print results
	"Square root of 2: " io::print
	root io::println

	"Current timestamp: " io::print
	t io::println
}
```

## Creating Your Own Modules

A module is simply a `.qd` file. Create `mymath.qd`:

```qd
// mymath.qd

fn square(x:i64 -- result:i64) {
	dup *
}

fn cube(x:i64 -- result:i64) {
	dup dup * *
}
```

Use it from another file:

```qd
use mymath

fn main( -- ) {
	5 mymath::square print nl  // 25
	3 mymath::cube print nl    // 27
}
```

## Module Structure

A typical module contains:

1. **Imports** at the top
2. **Constants** and **structs**
3. **Public functions**
4. **Helper functions**

```qd
// geometry.qd

use math

const PI 3.14159265358979

struct Circle {
	radius:f64
}

fn circle_area(r:f64 -- area:f64) {
	dup * PI *
}

fn circle_circumference(r:f64 -- c:f64) {
	2.0 * PI *
}
```

## Public vs Private

All functions in a module are accessible. Use naming conventions to indicate privacy:

```qd
// Convention: underscore prefix for "private" helpers
fn _helper(x:i64 -- y:i64) {
	// Internal use only
	1 +
}

fn public_function(x:i64 -- result:i64) {
	_helper 2 *
}
```

## Module Search Path

Quadrate looks for modules in:

1. Current directory
2. `$QUADRATE_ROOT` if set
3. Standard library locations

## Avoiding Name Conflicts

Module namespaces prevent conflicts:

```qd
use mylib
use otherlib

fn main( -- ) {
	// These are different functions
	5 mylib::process
	5 otherlib::process
}
```

## What's Next?

Now let's learn about [Control Flow](../4-control-flow/conditionals.md) - conditionals, loops, and more.
