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
use strings

fn main() {
	3.14159 math::sin print nl
	"hello" strings::len print nl
}
```

## Directory-based namespaces

Files in the same directory automatically share a namespace. This means you can split your code across multiple files without explicit imports:

```
myproject/
  main.qd      // Entry point
  helpers.qd   // Helper functions
  types.qd     // Struct definitions
```

```qd
// types.qd
struct Point {
	x:f64
	y:f64
}

fn point_add(a:Point b:Point -- c:Point) {
	-> b -> a
	Point { x = a @x b @x + y = a @y b @y + }
}
```

```qd
// main.qd
fn main() {
	// Point and point_add are automatically available
	Point { x = 1.0 y = 2.0 } -> p1
	Point { x = 3.0 y = 4.0 } -> p2
	p1 p2 point_add -> p3
	p3 @x print nl  // 4.0
}
```

No `use` statement is needed for files in the same directory.

## Standard library modules

These modules ship with Quadrate and are always available:

| Module | Purpose |
|--------|---------|
| `bits` | Bit manipulation |
| `bytes` | Byte buffer operations |
| `flag` | Command-line flag parsing |
| `fmt` | Formatted output |
| `io` | Input/output operations |
| `limits` | Numeric limits |
| `math` | Mathematical functions |
| `mem` | Memory management |
| `os` | Operating system interaction |
| `path` | File path operations |
| `rand` | Random numbers |
| `sb` | String builder |
| `signal` | Unix signal handling |
| `strings` | String manipulation |
| `strconv` | String conversion |
| `term` | Terminal colors and formatting |
| `testing` | Testing framework |
| `thread` | Threading primitives |
| `time` | Date and time |
| `tty` | Terminal detection and dimensions |
| `unicode` | Unicode utilities |
| `base64` | Base64 encoding/decoding |
| `crypto` | Cryptographic functions |
| `ct` | Compile-time utilities |
| `hex` | Hex encoding/decoding |
| `hof` | Higher-order function combinators |
| `http` | HTTP client and server |
| `json` | JSON parsing and generation |
| `log` | Logging |
| `net` | Networking (TCP/UDP) |
| `regex` | Regular expressions |
| `sort` | Sorting |
| `tls` | TLS/SSL |
| `uri` | URI parsing |
| `uuid` | UUID generation |

## External packages

Additional modules are available as separate packages via `quadpm`. Install them with `quadpm get`:

```bash
quadpm get https://git.sr.ht/~klahr/qdcompress
```

After installing, use them like any standard library module.

For projects with multiple dependencies, list them in a `qd.json` file and run `quadpm install`:

```json
{
	"name": "myproject",
	"dependencies": {
		"compress": "https://git.sr.ht/~klahr/qdcompress"
	}
}
```

```bash
quadpm install
```

| Package | Install |
|---------|---------|
| `compress` | `quadpm get https://git.sr.ht/~klahr/qdcompress` |
| `sqlite` | `quadpm get https://git.sr.ht/~klahr/qdsqlite` |

See [External Packages](../../external-packages.md) for full documentation and examples.

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

A module is a folder containing `.qd` files. All `.qd` files in the folder share the same namespace. Functions marked with `pub` are accessible from outside the module.

Create a folder `mymath/` with source files inside:

```qd
// mymath/helpers.qd

// Private helper (no pub keyword) - only visible within mymath/
fn square_internal(x:i64 -- result:i64) {
	dup *
}
```

```qd
// mymath/api.qd

pub fn square(x:i64 -- result:i64) {
	square_internal  // Can call functions from helpers.qd
}

pub fn cube(x:i64 -- result:i64) {
	dup square_internal *
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

## Public vs private

Use `pub` to mark symbols that can be accessed from outside the module:

```qd
// mymodule/mymodule.qd

pub fn public_function(x:i64 -- result:i64) {
	helper 2 *
}

pub const PUBLIC_VALUE = 42

pub struct PublicStruct {
	data:i64
}

// No pub - only accessible within this module's directory
fn helper(x:i64 -- y:i64) {
	1 +
}

const PRIVATE_VALUE = 99

struct PrivateStruct {
	secret:i64
}
```

Trying to access private symbols from outside the module will result in a compile error:

```
error: Function 'helper' in module 'mymodule' is private and cannot be accessed
       from outside the module. Mark it as 'pub fn' to export it.
```

## Module structure

A typical module contains:

1. **Imports** at the top (for external modules like `math`)
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
