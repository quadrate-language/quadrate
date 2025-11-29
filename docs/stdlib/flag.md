# flag

Command-line flag parsing.

## Structs

### Flag

Parsed command-line arguments.

## Functions

### parse

Parse arguments from read instruction.

**Signature:** `( argc:i64 -- )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `argc` | `i64` | Argument count from read |

| Return | Type | Description |
|--------|------|-------------|
| `Flag` | `struct` | on stack |

**Example:**

```qd
read flag::parse -> f
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

### bool

Check if a boolean flag exists.

**Signature:** `( f:ptr name:str -- present:i64 )`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `name` | `str` | Flag name (e.g., "--verbose") |

| Return | Type | Description |
|--------|------|-------------|
| `present` | `i64` | 1 if found, 0 otherwise |

**Example:**

```qd
f "--verbose" flag::bool if { "verbose" . nl }
```

---

### string

Get string value of a flag.

**Signature:** `( f:ptr name:str -- value:str )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `name` | `str` | Flag name |

| Return | Type | Description |
|--------|------|-------------|
| `value` | `str` | Flag value |

**Errors:**

- Flag not found

**Example:**

```qd
f "--name" flag::string! -> name
```

---

### int

Get integer value of a flag.

**Signature:** `( f:ptr name:str -- value:i64 )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `name` | `str` | Flag name |

| Return | Type | Description |
|--------|------|-------------|
| `value` | `i64` | Flag value as integer |

**Errors:**

- Flag not found

**Example:**

```qd
f "--count" flag::int! -> count
```

---

### float

Get float value of a flag.

**Signature:** `( f:ptr name:str -- value:f64 )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `name` | `str` | Flag name |

| Return | Type | Description |
|--------|------|-------------|
| `value` | `f64` | Flag value as float |

**Errors:**

- Flag not found

**Example:**

```qd
f "--rate" flag::float! -> rate
```

---

### positional

Get positional argument at index.

**Signature:** `( f:ptr index:i64 -- value:str )!`

| Parameter | Type | Description |
|-----------|------|-------------|
| `f` | `ptr` | Flag struct |
| `index` | `i64` | Position (0-based) |

| Return | Type | Description |
|--------|------|-------------|
| `value` | `str` | Positional argument |

**Errors:**

- Not implemented
