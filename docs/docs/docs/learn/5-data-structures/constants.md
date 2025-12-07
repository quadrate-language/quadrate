# Constants

Constants define fixed values that cannot change.

## Defining Constants

```qd
const PI 3.14159265358979
const MAX_SIZE 1000
const GREETING "Hello, World!"
```

Constants are defined at the top level, outside functions.

## Using Constants

```qd
const PI 3.14159265358979

fn circle_area(r:f64 -- area:f64) {
	-> r
	r dup * PI *
}

fn circle_circumference(r:f64 -- c:f64) {
	-> r
	2.0 r * PI *
}

fn main( -- ) {
	5.0 circle_area print nl        // 78.5398...
	5.0 circle_circumference print nl  // 31.4159...
}
```

## Constant Types

Constants can be any basic type:

```qd
// Integer constants
const MAX_RETRIES 3
const BUFFER_SIZE 4096
const INVALID_ID -1

// Float constants
const PI 3.14159265358979
const E 2.71828182845905
const EPSILON 0.00001

// String constants
const VERSION "1.0.0"
const APP_NAME "MyApp"
```

## Constants vs Variables

| Feature | Constant | Variable |
|---------|----------|----------|
| Defined at | Top level | Inside functions |
| Can change | No | Yes |
| Scope | Global | Function |
| Syntax | `const NAME value` | `value -> name` |

## Naming Convention

Use UPPER_CASE for constants:

```qd
const MAX_CONNECTIONS 100
const DEFAULT_TIMEOUT 30
const API_BASE_URL "https://api.example.com"
```

## Constants for Configuration

```qd
const DEBUG_MODE 1
const LOG_LEVEL 2
const MAX_THREADS 8

fn main( -- ) {
	DEBUG_MODE if {
		"Debug mode enabled" print nl
	}

	LOG_LEVEL 2 >= if {
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
const MIN_PORT 1
const MAX_PORT 65535

fn is_valid_port(port:i64 -- valid:i64) {
	-> port
	port MIN_PORT >= port MAX_PORT <= and
}
```

## Constants for Error Codes

```qd
const ERR_NONE 0
const ERR_NOT_FOUND 1
const ERR_PERMISSION 2
const ERR_TIMEOUT 3

fn handle_error(code:i64 -- ) {
	-> code
	code switch {
		ERR_NONE => { "Success" print nl }
		ERR_NOT_FOUND => { "Not found" print nl }
		ERR_PERMISSION => { "Permission denied" print nl }
		ERR_TIMEOUT => { "Timeout" print nl }
		_ => { "Unknown error" print nl }
	}
}
```

## Constants for Bit Flags

```qd
const FLAG_READ 1
const FLAG_WRITE 2
const FLAG_EXECUTE 4

fn has_read(flags:i64 -- result:i64) {
	-> flags
	flags FLAG_READ and 0 !=
}

fn has_write(flags:i64 -- result:i64) {
	-> flags
	flags FLAG_WRITE and 0 !=
}

fn main( -- ) {
	FLAG_READ FLAG_WRITE or -> permissions

	permissions has_read print nl   // 1
	permissions has_write print nl  // 1
}
```

## Computed Constants

Constants must be literal values. For computed values, use functions:

```qd
const HOURS_PER_DAY 24
const DAYS_PER_WEEK 7

fn hours_per_week( -- h:i64) {
	HOURS_PER_DAY DAYS_PER_WEEK *
}

fn main( -- ) {
	hours_per_week print nl  // 168
}
```

## Module Constants

Constants can be accessed from other modules:

```qd
// config.qd
const APP_VERSION "2.0.0"
const MAX_USERS 1000

// main.qd
use config

fn main( -- ) {
	"Version: " print config::APP_VERSION print nl
}
```

## Best Practices

1. **Name clearly**: `MAX_CONNECTIONS` not `MC`
2. **Group related constants**: Keep HTTP status codes together
3. **Document purpose**: Add comments for non-obvious values
4. **Use for repeated values**: If you use a value more than once

```qd
// HTTP Status Codes
const HTTP_OK 200
const HTTP_NOT_FOUND 404
const HTTP_SERVER_ERROR 500

// Timeouts (in seconds)
const CONNECT_TIMEOUT 30
const READ_TIMEOUT 60
const WRITE_TIMEOUT 60
```

## What's Next?

Learn about [Error Handling](../6-error-handling/basics.md) to write robust code.
