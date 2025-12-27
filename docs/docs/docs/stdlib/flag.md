# `use` flag

Command-line flag parsing.
Error codes: Ok=1 (success), specific errors start at 2

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `ErrInvalidValue` | `4` | Error: Invalid value format. |
| `ErrNotFound` | `2` | Error: Flag not found. |
| `ErrNoValue` | `3` | Error: Value not found (flag exists but has no value). |

## Functions

### `fn` boolean

Check if a boolean flag exists.

**Signature:** `(f:ptr name:str -- present:i64)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `name` | `str` | Flag name (e.g., "--verbose") |

| Output | Type | Description |
|--------|------|-------------|
| `present` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
f "--verbose" flag::boolean if { "verbose" print nl }
```
---

### `fn` destroy

Free a Flag struct and its argv string.

**Signature:** `(f:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct to free |

**Example:**

```qd
f flag::destroy
```
---

### `fn` float

Get float value of a flag.

**Signature:** `(f:ptr name:str -- value:f64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `name` | `str` | Flag name |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `f64` | Flag value as float |

| Error | Description |
|-------|-------------|
| `flag::ErrNotFound` | Flag not found |

**Example:**

```qd
f "--rate" flag::float!  // rate
```
---

### `fn` int

Get integer value of a flag.

**Signature:** `(f:ptr name:str -- value:i64)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `name` | `str` | Flag name |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Flag value as integer |

| Error | Description |
|-------|-------------|
| `flag::ErrNotFound` | Flag not found |

**Example:**

```qd
f "--count" flag::int!  // count
```
---

### `fn` parse

Parse arguments from read instruction.

**Signature:** `(argc:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `argc` | `i64` | Argument count from read |

| Output | Type | Description |
|--------|------|-------------|
| `Flag` | `struct` | on stack |

**Example:**

```qd
read flag::parse  // f
```
---

### `fn` positional

Get positional argument at index.

**Signature:** `(f:ptr index:i64 -- value:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `index` | `i64` | Position (0-based) |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Positional argument |

| Error | Description |
|-------|-------------|
| `flag::ErrNotFound` | Not yet implemented |
---

### `fn` string

Get string value of a flag.

**Signature:** `(f:ptr name:str -- value:str)!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `name` | `str` | Flag name |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Flag value |

| Error | Description |
|-------|-------------|
| `flag::ErrNotFound` | Flag not found |
| `flag::ErrNoValue` | Flag exists but has no value |

**Example:**

```qd
f "--name" flag::string!  // name
```
## Flag

Parsed command-line arguments.

### Struct

| Field | Type | Description |
|-------|------|-------------|
| `argc` | `i64` | Argument count |
| `argv` | `str` | Arguments string |

