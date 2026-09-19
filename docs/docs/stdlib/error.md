# `use` error

<!-- doccheck: page-context use error -->

<!-- doccheck: page-setup "disk full" 5 error::new -> e -->

Errors as values.

A fallible call reports failure through `!`, `?`, or an `if`/`switch` arm, and the message
and code it failed with live in a channel the arm reads with the `err` builtin. That channel
hands back two loose values and empties itself as it is read, so what a program gets is a
pair it cannot keep together: an error cannot be stored, returned, collected, or given
context on the way past. `Error` is that pair as one ordinary value.

Read it first in the failure arm. `err` empties the channel, and the next fallible call
clears it, so `error::last` has to run before anything else that can fail.

This module is a thin layer over the channel, not a replacement for it: raising is still
`panic`, and a call site still says `!`, `?`, or reads the result with `if` or `switch`.

@example
use error

fn parse_port(s:str -- port:i64)! {
    s "" == if { "port is empty" 1 panic }
    8080
}

fn main() {
    "" parse_port if {
        print nl
    } else {
        error::last -> e
        e <<message print nl
    }
}

## Error

A failure: the code an `if` or `switch` arm matched on, the message `panic` was given, and the error this one was wrapped around, if there is one.  Fields:   code    - the code passed to `panic`; 0 when nothing has failed   message - the message passed to `panic`, with any wrapping context already composed in   cause   - the error this one wrapped, or null

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `code` | `i64` |  |
| `message` | `str` |  |

### Constructors

#### `fn` last

The error the last fallible call failed with, as one value.  Reading empties the channel, so call this once, first, in the failure arm. With nothing to report it answers with code 0 and an empty message.

**Signature:** `( -- e:Error)`

| Output | Type | Description |
|--------|------|-------------|
| `e` | `Error` | The error |

**Example:**

```qd
error::last  // failure
```
---

#### `fn` new

Build an error without raising one, for a function that returns a failure as a value.

**Signature:** `(msg:str code:i64 -- e:Error)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `msg` | `str` | The message |
| `code` | `i64` | The code |

| Output | Type | Description |
|--------|------|-------------|
| `e` | `Error` | The error |

**Example:**

```qd
"disk full" 5 error::new  // e
```

### Methods

#### `fn` root

The error at the bottom of the cause chain -- the one that actually failed.

**Signature:** `(e:Error) root( -- r:Error)`

| Output | Type | Description |
|--------|------|-------------|
| `r` | `Error` | The root cause, or the error itself when it wrapped nothing |

**Example:**

```qd
e error::root <<code  // original_code
```
---

#### `fn` wrap

Wrap an error in context from the frame that caught it.  The wrapped error keeps the original's code, so a caller matching on it in a `switch` still matches; the message reads "context: original"; and `cause` holds the error wrapped. Re-raise the result with `panic`, which the compiler reads as diverging, so the arm needs nothing after it: `e <<message e <<code panic`.

**Signature:** `(e:Error) wrap(msg:str -- wrapped:Error)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `msg` | `str` | The context to add |

| Output | Type | Description |
|--------|------|-------------|
| `wrapped` | `Error` | The wrapped error |

**Example:**

```qd
e "cannot save the document" error::wrap  // wrapped
```

