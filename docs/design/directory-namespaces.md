# Directory-Based Namespace System

## Overview

Adopt a Hare-like namespace model where directory = namespace. All `.qd` files in the same directory automatically share a namespace without explicit imports between them.

## Core Rules

### 1. Directory = Namespace

All `.qd` files in the same directory form a single namespace:

```
project/
  main.qd          # namespace: main (entry point)

rocket/
  constants.qd     # namespace: rocket
  state.qd         # namespace: rocket
  physics.qd       # namespace: rocket
  integrator.qd    # namespace: rocket
```

### 2. Namespace Naming

- **Entry point directory**: The directory containing the compiled file is the `main` namespace
- **Other directories**: Namespace name = directory name
- **Nested directories**: Each directory is its own namespace (flat, not hierarchical)

Example:
```
myapp/
  main.qd          # Compiling this: namespace is "main"
  helper.qd        # Also "main" namespace (same directory)

utils/
  strings.qd       # namespace: utils
  numbers.qd       # namespace: utils
```

### 3. Same-Namespace Access

Files in the same namespace can access each other's symbols directly - no imports needed:

```quadrate
// rocket/state.qd
struct State { pos:math::Vec3 vel:math::Vec3 }
fn altitude(s:State -- alt:f64) { ... }

// rocket/physics.qd
use math                    // External namespace - still needed

fn compute(s:State -- ) {   // State visible - same namespace
    s altitude -> alt       // altitude visible - same namespace
    s @vel length -> v      // math::Vec3 method works
}
```

### 4. External Namespace Access

Access other namespaces with `use` - always requires qualified names:

```quadrate
// main.qd
use rocket                  // Import rocket namespace

fn main() {
    rocket::State { ... } -> s
    s rocket::compute
}
```

### 5. Visibility with `pub`

- `pub fn`, `pub struct`, `pub const` - visible outside namespace
- No `pub` - internal to namespace only

```quadrate
// rocket/state.qd
pub struct State { ... }           // Visible to other namespaces
pub fn altitude(s:State -- ) { }   // Visible to other namespaces
fn internal_helper( -- ) { }       // Only visible within rocket/

// main.qd
use rocket
rocket::State { }       // OK - pub
rocket::altitude        // OK - pub
rocket::internal_helper // ERROR - not pub
```

### 6. Standard Library

Standard library modules continue to work as before:

```quadrate
use math
use fmt
use io

math::sqrt
fmt::printf
```

## Compilation Model

### When compiling `quadc rocket/main.qd`:

1. Collect all `.qd` files in `rocket/` directory
2. These form the `main` namespace (entry point)
3. All symbols visible to each other within namespace
4. Parse and validate as a unit

### When `use rocket` is encountered:

1. Find `rocket/` directory (relative to source, or in include paths)
2. Collect all `.qd` files in `rocket/`
3. These form the `rocket` namespace
4. Only `pub` symbols are accessible from outside

## Directory Resolution

When resolving `use foo`:

1. Check for `foo/` directory relative to current source file
2. Check for `foo/` in include paths (`-I` flags)
3. Check for standard library module `foo`

## Edge Cases

### Single file in directory
Still forms a namespace. A single `utils/helper.qd` is the `utils` namespace.

### No `main()` function
Error if compiling as entry point. OK if used as library namespace.

### Circular namespace dependencies
`use` creates a dependency. Circular `use` between namespaces is an error:
```
// a/foo.qd
use b        // ERROR if b also uses a

// b/bar.qd
use a
```

### File with same name as directory
`rocket.qd` and `rocket/` both exist - directory takes precedence for `use rocket`.

## Migration from `use "file.qd"`

### Phase 1: Warning
```
warning: 'use "file.qd"' is deprecated, use directory-based namespaces
```

### Phase 2: Remove
Remove support entirely in future version.

### Migration example

Before:
```quadrate
// main.qd
use "state.qd"
use "physics.qd"

State { } -> s
s compute
```

After (all files in same directory):
```quadrate
// main.qd
// No imports needed for same-directory files

State { } -> s
s compute
```

Or if in different directory:
```quadrate
// main.qd
use rocket

rocket::State { } -> s
s rocket::compute
```

## Implementation Changes

1. **quadc**: Collect all `.qd` files in directory when compiling
2. **SemanticValidator**: Handle multi-file namespace, check `pub` visibility
3. **LlvmGenerator**: Generate code for namespace as unit
4. **Parser**: No changes needed (already parses `pub`)

## Example: Rocket Simulation

### New structure (no `use "file.qd"`):

```
examples/rocket/
  main.qd         # Entry point, has main()
  constants.qd    # pub const EARTH_RADIUS, etc.
  state.qd        # pub struct State, pub fn altitude
  stage.qd        # pub struct Stage, pub fn stage_thrust
  physics.qd      # pub fn compute_deriv
  integrator.qd   # pub fn step
  orbital.qd      # pub fn eccentricity, apoapsis, etc.
```

Each file removes `use "file.qd"` imports. All symbols accessible within `rocket/` namespace.

When compiled with `quadc examples/rocket/main.qd`, all files are collected and compiled as the `main` namespace.
