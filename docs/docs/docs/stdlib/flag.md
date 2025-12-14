# flag

Command-line flag parsing.

Error codes: `Ok` (1) for success, specific errors start at 2.

## Constants

### Error Codes

| Name | Value | Description |
|------|-------|-------------|
| `ErrNotFound` | `2` | Error: Flag not found. |
| `ErrNoValue` | `3` | Error: Value not found (flag exists but has no value). |
| `ErrInvalidValue` | `4` | Error: Invalid value format. |

## Structs

### Flag

Parsed command-line arguments.

## Functions

### boolean

Check if a boolean flag exists.

**Signature:** `( f:ptr name:str -- present:i64 )`

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

### destroy

Free a Flag struct and its argv string.

**Signature:** `( f:ptr -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct to free |

**Example:**

```qd
f flag::destroy
```

---

### float

Get float value of a flag.

**Signature:** `( f:ptr name:str -- value:f64 )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `name` | `str` | Flag name |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `f64` | Flag value as float |

**Errors:**

- Flag not found

**Example:**

```qd
f "--rate" flag::float!  // rate
```

---

### int

Get integer value of a flag.

**Signature:** `( f:ptr name:str -- value:i64 )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `name` | `str` | Flag name |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Flag value as integer |

**Errors:**

- Flag not found

**Example:**

```qd
f "--count" flag::int!  // count
```

---

### parse

Parse arguments from read instruction.

**Signature:** `( argc:i64 -- )`

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

### positional

Get positional argument at index.

**Signature:** `( f:ptr index:i64 -- value:str )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `index` | `i64` | Position (0-based) |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Positional argument |

**Errors:**

- Not implemented

---

### string

Get string value of a flag.

**Signature:** `( f:ptr name:str -- value:str )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `name` | `str` | Flag name |

| Output | Type | Description |
|--------|------|-------------|
| `value` | `str` | Flag value |

**Errors:**

- Flag not found

**Example:**

```qd
f "--name" flag::string!  // name
```
