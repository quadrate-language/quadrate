# Constants & Enums

Constants and enums define fixed values that cannot change.

## Defining constants

```qd
const Pi = 3.14159265358979
const MaxSize = 1000
const Greeting = "Hello, World!"
```

Constants are defined at the top level, outside functions.

## Using constants

```qd
const Pi = 3.14159265358979

fn circle_area(r:f64 -- area:f64) {
	r dup * Pi *
}

fn circle_circumference(r:f64 -- c:f64) {
	2.0 r * Pi *
}

fn main() {
	5.0 circle_area print nl           // 78.5398...
	5.0 circle_circumference print nl  // 31.4159...
}
```

## Constant types

Constants can be any basic type:

```qd
// Integer constants
const MaxRetries = 3
const BufferSize = 4096
const InvalidId = -1

// Float constants
const Pi = 3.14159265358979
const E = 2.71828182845905
const Epsilon = 0.00001

// String constants
const Version = "1.0.0"
const AppName = "MyApp"
```

## Constants vs variables

| Feature | Constant | Variable |
|---------|----------|----------|
| Defined at | Top level | Inside functions |
| Can change | No | Yes |
| Scope | Global | Function |
| Syntax | `const Name = value` | `value -> name` |

## Built-in constants

| Constant | Value | Type | Description |
|----------|-------|------|-------------|
| `true` | 1 | i64 | Boolean true |
| `false` | 0 | i64 | Boolean false |
| `Ok` | 1 | i64 | Success result |
| `Err` | 0 | i64 | Error result |
| `null` | 0 | ptr | Null pointer |

Use `null` for pointer fields in structs:

```qd
struct Node {
	value:i64
	next:*Node
}

Node { value = 1 next = null } -> a
a <<next null == if { "end of list" print nl }
```

## Naming convention

Use PascalCase for constants:

```qd
const MaxConnections = 100
const DefaultTimeout = 30
const ApiBaseUrl = "https://api.example.com"
```

## Constants for configuration

```qd
const DebugMode = 1
const LogLevel = 2
const MaxThreads = 8

fn main() {
	DebugMode if {
		"Debug mode enabled" print nl
	}

	LogLevel 2 >= if {
		"Verbose logging" print nl
	}
}
```

## Constants for magic numbers

Replace magic numbers with named constants:

### Before (unclear)

```qd
fn is_valid_port(port:i64 -- valid:i64) {
	port 0 > port 65535 <= and
}
```

### After (clear)

```qd
const MinPort = 1
const MaxPort = 65535

fn is_valid_port(port:i64 -- valid:i64) {
	port MinPort >= port MaxPort <= and
}
```

## Constants for error codes

```qd
const ErrNone = 0
const ErrNotFound = 1
const ErrPermission = 2
const ErrTimeout = 3

fn handle_error(code:i64 -- ) {
	code switch {
		0 { "Success" print nl }
		1 { "Not found" print nl }
		2 { "Permission denied" print nl }
		3 { "Timeout" print nl }
		_ { "Unknown error" print nl }
	}
}
```

!!! note "Constants in switch"
    Constants cannot currently be used as `switch` case values. Use literal values instead.

## Constants for bit flags

```qd
const FlagRead = 1
const FlagWrite = 2
const FlagExecute = 4

fn has_read(flags:i64 -- result:i64) {
	flags FlagRead and 0 !=
}

fn has_write(flags:i64 -- result:i64) {
	flags FlagWrite and 0 !=
}

fn main() {
	FlagRead FlagWrite or -> permissions

	permissions has_read print nl   // 1
	permissions has_write print nl  // 1
}
```

## Computed constants

Constants must be literal values. For computed values, use functions:

```qd
const HoursPerDay = 24
const DaysPerWeek = 7

fn hours_per_week( -- h:i64) {
	HoursPerDay DaysPerWeek *
}

fn main() {
	hours_per_week print nl  // 168
}
```

## Module constants

Constants can be accessed from other modules:

```qd
// config/config.qd
pub const AppVersion = "2.0.0"
pub const MaxUsers = 1000
```

```qd
// main.qd
use config

fn main() {
	"Version: " print config::AppVersion print nl
}
```

## Enums

Enums define a set of named integer constants under a common scope. Values auto-increment from 0, or can be set explicitly.

```qd
enum Color {
	Red        // 0
	Green      // 1
	Blue       // 2
}

enum HttpStatus {
	Ok = 200
	NotFound = 404
	ServerError = 500
}
```

### Using enums

Enum values are accessed with `EnumName::Variant` and are regular `i64` values:

```qd
enum Direction {
	Up
	Down
	Left
	Right
}

fn describe(d:i64 -- ) {
	d switch {
		Direction::Up { "up" print nl }
		Direction::Down { "down" print nl }
		Direction::Left { "left" print nl }
		Direction::Right { "right" print nl }
		_ { "unknown" print nl }
	}
}

fn main() {
	Direction::Up describe        // up
	Direction::Left describe      // left

	// Enums are i64 — arithmetic and comparison work
	Direction::Up Direction::Down == if {
		"same" print nl
	} else {
		"different" print nl
	}
}
```

### Explicit values

Variants auto-increment from the previous value. Explicit values reset the counter:

```qd
enum Token {
	Int            // 0
	Float          // 1
	Str = 10       // 10
	Ident          // 11
	Plus = 20      // 20
	Minus          // 21
	Eof = -1       // -1
}
```

### Public enums

Use `pub enum` to export from a module:

```qd
// status.qd
pub enum Status {
	Ok
	Error = -1
	Pending = 100
}
```

## What's next?

Learn about [Error Handling](../6-error-handling/basics.md) to write robust code.
