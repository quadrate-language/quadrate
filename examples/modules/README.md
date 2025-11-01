# Modules Example

This example demonstrates Quadrate's module system, which allows you to organize code into reusable modules.

## File Structure

```
examples/modules/
├── main.qd                   # Main program that uses the module
└── math_utils/               # Module directory
    └── module.qd             # Module implementation
```

## How Modules Work

### 1. Module Discovery

When you write `use math_utils`, the compiler searches for the module in this order:

1. **Local path**: `./math_utils/module.qd` (relative to your source file)
2. **QUADRATE_ROOT**: `$QUADRATE_ROOT/math_utils/module.qd`
3. **Home directory**: `$HOME/quadrate/math_utils/module.qd`

### 2. Module Compilation

The compiler automatically:
- Discovers all imported modules
- Transpiles each module to C (`math_utils/module.qd.c`)
- Generates a header file (`math_utils/module.h`)
- Compiles module to an object file
- Links everything together

### 3. Calling Module Functions

Use the `module::function` syntax to call module functions:

```quadrate
use math_utils

fn main() {
    math_utils::pi print       // Call pi() from math_utils module
}
```

## Compilation Process

When you compile `main.qd`, the compiler:

1. **Validates & Type Checks** - Checks that imported modules exist, functions are defined, and analyzes type safety
2. **Transpiles** - Converts both main and module files to C
3. **Generates Headers** - Creates `math_utils/module.h` with function declarations
4. **Compiles** - Compiles all C files to object files
5. **Links** - Links everything into a single executable

## Generated Files (with --save-temps)

```
/tmp/qd_xxxxx/
├── main/
│   ├── main.qd.c         # Transpiled main program
│   └── main.qd.c.o       # Compiled object file
├── math_utils/
│   ├── module.qd.c       # Transpiled module
│   ├── module.h          # Generated header file
│   └── module.qd.c.o     # Compiled object file
└── main.c                 # Entry point (calls usr_main_main)
```

## Module Naming Convention

In the generated C code, module functions are prefixed with `usr_<module>_`:

- `math_utils::pi` becomes `usr_math_utils_pi(ctx)`
- `math_utils::tau` becomes `usr_math_utils_tau(ctx)`

## Running the Example

```bash
# From the repository root:
quadc examples/modules/main.qd -r

# Compile and save temporary files:
quadc examples/modules/main.qd --save-temps -o module_demo

# Run with verbose output:
quadc examples/modules/main.qd --verbose -r
```

## Type Checking

The semantic validator fully supports type checking for module functions:
- Module functions can be called with `module::function` syntax
- Validation checks that modules are imported and functions exist
- Type checking analyzes module function signatures and tracks stack effects
- Type errors are reported at compile time

The type checker automatically:
1. Loads and parses imported module files
2. Analyzes each module function's stack effect signature
3. Tracks types through module function calls
4. Reports type errors if stack types don't match

## Creating Your Own Modules

1. Create a directory with your module name: `mymodule/`
2. Create `module.qd` inside it with your functions
3. In your main program: `use mymodule`
4. Call functions with: `mymodule::function_name`

Example:

```quadrate
// mymodule/module.qd
fn helper() {
    42
}

// main.qd
use mymodule

fn main() {
    mymodule::helper print
}
```
