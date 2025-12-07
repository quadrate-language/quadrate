# Context (ctx)

The context system allows passing values through the call stack without explicit parameters.

## What is Context?

Context is a way to share data across functions without passing it as arguments. It's useful for:

- Request-scoped data (user ID, request ID)
- Configuration values
- Logging contexts
- Database connections

## Getting Context

Use `ctx` to get a value from the context:

```qd
fn greet( -- ) {
	"username" ctx -> name
	"Hello, " print name print nl
}
```

## Setting Context

Use `with` to set a context value for a scope:

```qd
fn main( -- ) {
	"Alice" "username" with {
		greet  // "Hello, Alice"
	}

	"Bob" "username" with {
		greet  // "Hello, Bob"
	}
}
```

## Context Propagation

Context flows down the call stack:

```qd
fn inner( -- ) {
	"user" ctx -> user
	"Inner: " print user print nl
}

fn middle( -- ) {
	inner
}

fn outer( -- ) {
	"admin" "user" with {
		middle
	}
}

fn main( -- ) {
	outer  // "Inner: admin"
}
```

## Nested Contexts

Inner contexts shadow outer ones:

```qd
fn show( -- ) {
	"value" ctx -> v
	v print nl
}

fn main( -- ) {
	1 "value" with {
		show  // 1

		2 "value" with {
			show  // 2 (shadows outer)
		}

		show  // 1 (back to outer)
	}
}
```

## Multiple Context Values

Set multiple values:

```qd
fn process( -- ) {
	"user_id" ctx -> uid
	"request_id" ctx -> rid

	"Processing request " print rid print
	" for user " print uid print nl
}

fn main( -- ) {
	123 "user_id" with {
		"req-456" "request_id" with {
			process
		}
	}
}
```

## Default Values

Check if context exists:

```qd
fn get_user_or_default( -- user:str) {
	"user" ctx -> user
	user 0 == if {
		"anonymous"
	} else {
		user
	}
}
```

## Common Patterns

### Request Context

```qd
struct RequestContext {
	user_id:i64
	trace_id:str
	start_time:i64
}

fn handle_request(ctx:ptr -- ) {
	-> ctx
	ctx "request" with {
		process_request
	}
}

fn process_request( -- ) {
	"request" ctx -> ctx
	ctx @user_id -> user_id
	// ... handle request
}
```

### Logging Context

```qd
fn log(msg:str -- ) {
	-> msg
	"prefix" ctx -> prefix
	prefix 0 != if {
		"[" print prefix print "] "
	}
	msg print nl
}

fn main( -- ) {
	"startup" log

	"AUTH" "prefix" with {
		"checking credentials" log
		"user authenticated" log
	}

	"done" log
}
// Output:
// startup
// [AUTH] checking credentials
// [AUTH] user authenticated
// done
```

### Database Connection

```qd
fn with_database(dsn:str -- )! {
	-> dsn
	dsn db_connect if {
		-> conn
		defer { conn db_close }

		conn "db" with {
			// All queries in this block use conn
			run_queries
		}
	} else {
		drop
		"connection failed" 1 error
	}
}

fn run_queries( -- ) {
	"db" ctx -> conn
	// Use conn for queries...
}
```

## Context vs Parameters

Use context when:

- Value is used by many functions in the call chain
- You don't want to thread a parameter through every function
- The value is "ambient" (like current user, logger, etc.)

Use parameters when:

- Value is directly used by the function
- Function signature should document its dependencies
- Testing is easier with explicit parameters

## What's Next?

Learn about [Function Pointers](function-pointers.md) for dynamic dispatch.
