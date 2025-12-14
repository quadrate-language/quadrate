# Constants

Constants define fixed values that cannot change.

## Defining Constants

```qd
const Pi = 3.14159265358979
const MaxSize = 1000
const Greeting = "Hello, World!"
```

Constants are defined at the top level, outside functions.

## Using Constants

```qd
const Pi = 3.14159265358979

fn circle_area(r:f64 -- area:f64) {
	-> r
	r dup * Pi *
}

fn circle_circumference(r:f64 -- c:f64) {
	-> r
	2.0 r * Pi *
}

fn main() {
	5.0 circle_area print nl           // 78.5398...
	5.0 circle_circumference print nl  // 31.4159...
}
```

## Constant Types

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

## Constants vs Variables

| Feature | Constant | Variable |
|---------|----------|----------|
| Defined at | Top level | Inside functions |
| Can change | No | Yes |
| Scope | Global | Function |
| Syntax | `const Name = value` | `value -> name` |

## Naming Convention

Use PascalCase for constants:

```qd
const MaxConnections = 100
const DefaultTimeout = 30
const ApiBaseUrl = "https://api.example.com"
```

## Constants for Configuration

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

## Constants for Magic Numbers

Replace magic numbers with named constants:

### Before (unclear)

```qd
fn is_valid_port(port:i64 -- valid:i64) {
	-> port
	port 0 > port 65535 <= and
}
```

### After (clear)

```qd
const MinPort = 1
const MaxPort = 65535

fn is_valid_port(port:i64 -- valid:i64) {
	-> port
	port MinPort >= port MaxPort <= and
}
```

## Constants for Error Codes

```qd
const ErrNone = 0
const ErrNotFound = 1
const ErrPermission = 2
const ErrTimeout = 3

fn handle_error(code:i64 -- ) {
	-> code
	code switch {
		ErrNone {
			"Success" print nl
		}
		ErrNotFound {
			"Not found" print nl
		}
		ErrPermission {
			"Permission denied" print nl
		}
		ErrTimeout {
			"Timeout" print nl
		}
		_ {
			"Unknown error" print nl
		}
	}
}
```

## Constants for Bit Flags

```qd
const FlagRead = 1
const FlagWrite = 2
const FlagExecute = 4

fn has_read(flags:i64 -- result:i64) {
	-> flags
	flags FlagRead and 0 !=
}

fn has_write(flags:i64 -- result:i64) {
	-> flags
	flags FlagWrite and 0 !=
}

fn main() {
	FlagRead FlagWrite or -> permissions

	permissions has_read print nl   // 1
	permissions has_write print nl  // 1
}
```

## Computed Constants

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

## Module Constants

Constants can be accessed from other modules:

```qd
// config.qd
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

## Best Practices

1. **Name clearly**: `MaxConnections` not `MC`
2. **Group related constants**: Keep HTTP status codes together
3. **Document purpose**: Add comments for non-obvious values
4. **Use for repeated values**: If you use a value more than once

```qd
// HTTP Status Codes
const HttpOk = 200
const HttpNotFound = 404
const HttpServerError = 500

// Timeouts (in seconds)
const ConnectTimeout = 30
const ReadTimeout = 60
const WriteTimeout = 60
```

## What's Next?

Learn about [Error Handling](../6-error-handling/basics.md) to write robust code.
