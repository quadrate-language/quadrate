# Modules

Modules organize code into reusable units. They help structure larger programs.

## Importing modules

Use `use` to import a module:

```qd
use fmt

fn main() {
	"John Doe" "Hello %s" fmt::printf nl
}
```

The `use` statement makes the module available via its namespace.

## Calling module functions

Use `::` to call functions from a module:

```qd
use math
use str

fn main() {
	3.14159 math::sin print nl
	"hello" str::len print nl
}
```

## Standard library modules

Quadrate includes many built-in modules:

| Module | Purpose |
|--------|---------|
| `base64` | Base64 encoding/decoding |
| `bits` | Bit manipulation |
| `bytes` | Byte buffer operations |
| `crc32` | CRC32 checksums |
| `flag` | Command-line flag parsing |
| `fmt` | Formatted output |
| `hex` | Hexadecimal encoding/decoding |
| `io` | Input/output operations |
| `json` | JSON parsing |
| `math` | Mathematical functions |
| `mem` | Memory management |
| `net` | Networking |
| `os` | Operating system interaction |
| `path` | File path operations |
| `rand` | Random numbers |
| `regex` | Regular expressions |
| `sb` | String builder |
| `sha256` | SHA-256 hashing |
| `sort` | Sorting functions |
| `str` | String manipulation |
| `strconv` | String conversion |
| `testing` | Testing framework |
| `time` | Date and time |
| `unicode` | Unicode utilities |
| `uri` | URI parsing and building |
| `uuid` | UUID generation |

## Example: using multiple modules

```qd
use math
use time

fn main() {
	// Get current time
	time::now -> t

	// Do some math
	2.0 math::sqrt -> root

	// Print results
	"Square root of 2: " print root print nl

	"Current timestamp: " print t print nl
}
```

## Creating your own modules

A module is a folder containing `.qd` files. All `.qd` files in the folder are included as part of the module. Functions marked with `pub` are accessible from outside.

Create a folder `mymath/` with source files inside:

```qd
// mymath/math.qd

pub fn square(x:i64 -- result:i64) {
	dup *
}

pub fn cube(x:i64 -- result:i64) {
	dup dup * *
}

// Private helper (no pub keyword)
fn helper(x:i64 -- y:i64) {
	1 +
}
```

Use it from another file:

```qd
use mymath

fn main() {
	5 mymath::square print nl  // 25
	3 mymath::cube print nl    // 27
}
```

## Including single files

You can also include a single `.qd` file directly:

```qd
// utils.qd
pub fn double(x:i64 -- result:i64) {
	2 *
}
```

```qd
use utils.qd

fn main() {
	5 utils::double print nl  // 10
}
```

Note the `.qd` extension in the `use` statement.

## Public vs private

Use `pub` to mark functions that can be called from outside the module:

```qd
// mymodule/mymodule.qd

pub fn public_function(x:i64 -- result:i64) {
	helper 2 *
}

// No pub - only callable within this module
fn helper(x:i64 -- y:i64) {
	1 +
}
```

## Module structure

A typical module contains:

1. **Imports** at the top
2. **Constants** and **structs**
3. **Public functions** (marked with `pub`)
4. **Private helper functions**

```qd
// geometry/geometry.qd
use math

pub const Phi = 1.61803398874989484820

pub struct Circle {
	radius:f64
}

pub fn circle_area(r:f64 -- area:f64) {
	square math::Pi *
}

pub fn circle_circumference(r:f64 -- c:f64) {
	2.0 * math::Pi *
}

// Private helper
fn square(x:f64 -- result:f64) {
	dup *
}
```

## Module search path

Quadrate looks for modules in:

1. Current directory
2. `$QUADRATE_ROOT` if set
3. Standard library locations

## Avoiding name conflicts

Module namespaces prevent conflicts:

```qd
use mylib
use otherlib

fn main() {
	// These are different functions
	5 mylib::process
	5 otherlib::process
}
```

## What's next?

Now let's learn about [Control Flow](../4-control-flow/conditionals.md) - conditionals, loops, and more.
