# Tutorial: Creating Distributable Modules

This tutorial covers creating reusable modules that can be shared and installed by others. You'll learn the complete workflow from creating a module to publishing it.

## Module Structure

A distributable module has this structure:

```
my-module/
    module.qd        # Main module file (required)
    quadrate.toml    # Package manifest (required)
    README.md        # Documentation (recommended)
    LICENSE          # License file (recommended)
    examples/        # Example programs (optional)
        simple.qd
        demo.qd
    src/             # C source files (optional, for FFI)
        helper.c
```

## Step 1: Create the Module Directory

Create a new directory for your module:

```bash
mkdir qd-mymodule
cd qd-mymodule
git init
```

## Step 2: Write module.qd

Create `module.qd` with your module's functions. Mark public functions with `pub`:

```qd
// Math utilities module
// Provides additional math functions

/// Calculate the square of a number
pub fn square(x:i64 -- result:i64) {
    dup *
}

/// Calculate the cube of a number
pub fn cube(x:i64 -- result:i64) {
    dup dup * *
}

/// Check if a number is even
pub fn is_even(x:i64 -- result:i64) {
    2 % 0 =
}

/// Check if a number is odd
pub fn is_odd(x:i64 -- result:i64) {
    2 % 0 !=
}

// Private helper function (no pub keyword)
fn helper(x:i64 -- y:i64) {
    1 +
}
```

Key points:

- **`pub fn`** marks functions that users can call
- Functions without `pub` are private to the module
- Add doc comments with `///` for each public function

## Step 3: Create quadrate.toml

Create the package manifest `quadrate.toml`:

```toml
[package]
name = "mymodule"
version = "1.0.0"
description = "Math utilities for Quadrate"
license = "MIT"

[dependencies]
# No dependencies for this module
```

The `name` field determines how users import your module:

```qd
use mymodule

fn main() {
    5 mymodule::square print nl  // 25
}
```

## Step 4: Add Examples

Create an `examples/` directory with usage examples:

```bash
mkdir examples
```

Create `examples/simple.qd`:

```qd
use mymodule

fn main() {
    "5 squared: " print 5 mymodule::square print nl
    "3 cubed: " print 3 mymodule::cube print nl
    "4 is even: " print 4 mymodule::is_even print nl
    "7 is odd: " print 7 mymodule::is_odd print nl
}
```

## Step 5: Add README.md

Create `README.md` with documentation:

```markdown
# mymodule - Math Utilities for Quadrate

Additional math functions for Quadrate programs.

## Installation

```bash
quadpm get https://git.sr.ht/~yourname/qd-mymodule
```

## Usage

```quadrate
use mymodule

fn main() {
    5 mymodule::square print nl  // 25
    3 mymodule::cube print nl    // 27
}
```

## API Reference

### `mymodule::square(x:i64 -- result:i64)`
Returns x squared.

### `mymodule::cube(x:i64 -- result:i64)`
Returns x cubed.

### `mymodule::is_even(x:i64 -- result:i64)`
Returns 1 if x is even, 0 otherwise.

### `mymodule::is_odd(x:i64 -- result:i64)`
Returns 1 if x is odd, 0 otherwise.

## License

MIT
```

## Step 6: Test Your Module

During development, use the `-I` flag to tell the compiler where to find your module:

```bash
# From within your module directory
quadc -I . -r examples/simple.qd

# Or from anywhere using the full path
quadc -I /path/to/qd-mymodule -r /path/to/qd-mymodule/examples/simple.qd
```

The `-I` flag adds a module search path. The compiler reads `quadrate.toml` to match the module name, so your directory can be named anything (e.g., `qd-mymodule`) while the module is imported by its package name (e.g., `mymodule`).

You can specify multiple `-I` paths if your module depends on other local modules:

```bash
quadc -I . -I ../other-module -r examples/simple.qd
```

## Step 7: Publish

Commit and push to a Git repository:

```bash
git add .
git commit -m "Initial release"
git remote add origin https://git.sr.ht/~yourname/qd-mymodule
git push -u origin master
```

Users can now install your module:

```bash
quadpm get https://git.sr.ht/~yourname/qd-mymodule
```

Or with a specific version tag:

```bash
quadpm get https://git.sr.ht/~yourname/qd-mymodule@1.0.0
```

## Adding C Code (FFI)

For performance-critical code or system integration, you can add C functions to your module.

### Step 1: Create src/ Directory

```bash
mkdir src
```

### Step 2: Write the C Code

Create `src/helper.c`:

```c
#include <qdrt/ffi.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// Check if stdout is a terminal
qd_exec_result is_terminal(qd_context* ctx) {
    int result = isatty(STDOUT_FILENO);
    qd_stack_push_int(ctx->st, result);
    return (qd_exec_result){0};
}

// Check if an environment variable is set
qd_exec_result env_is_set(qd_context* ctx) {
    qd_stack_element_t elem;
    qd_stack_pop(ctx->st, &elem);

    const char* name = qd_string_data(elem.value.s);
    int result = getenv(name) != NULL;

    qd_string_release(elem.value.s);
    qd_stack_push_int(ctx->st, result);
    return (qd_exec_result){0};
}
```

Key requirements:

- Include `<qdrt/ffi.h>` for all FFI types
- Function signature: `qd_exec_result function_name(qd_context* ctx)`
- Pop arguments from stack, push results
- Release strings after use with `qd_string_release()`
- Return `(qd_exec_result){0}` for success

### Step 3: Import in module.qd

Add an import block to `module.qd`:

```qd
// Import C functions from static library
import "libmymodule_static.a" as "native" {
    fn is_terminal( -- result:i64)
    fn env_is_set(name:str -- result:i64)
}

// Wrap native functions with Quadrate interface
pub fn is_tty( -- result:i64) {
    native::is_terminal
}

pub fn has_env(name:str -- result:i64) {
    native::env_is_set
}
```

The import block:

- `"libmymodule_static.a"` - library filename (will be built by user)
- `as "native"` - namespace for imported functions
- Function signatures must match the C implementation

### Step 4: Build and Test

Use `quadpm build` to compile C sources during development:

```bash
quadpm build
```

This compiles all `.c` files in `src/` and creates:

- `lib/libmymodule.so` - shared library
- `lib/libmymodule_static.a` - static library

Then test your module using the `-I` flag:

```bash
quadc -I . -r examples/simple.qd
```

The typical development cycle is:

```bash
# 1. Edit C code in src/
# 2. Rebuild the library
quadpm build

# 3. Test your changes
quadc -I . -r examples/simple.qd
```

When users install your module with `quadpm get`, the C sources are automatically compiled.

### Step 5: Link with External C Libraries (Optional)

If your C code depends on external system libraries (like OpenGL, SDL, or SQLite), declare them in `quadrate.toml`:

```toml
[package]
name = "mymodule"
version = "1.0.0"
description = "My module with native dependencies"
license = "MIT"

[native]
link = ["GL", "GLU", "glut"]
```

The `[native]` section specifies:

- `link` - List of system libraries to link with (passed as `-l` flags to the linker)

For example, a GLUT wrapper module would have:

```toml
[native]
link = ["glut", "GL", "GLU"]
```

This tells the compiler to link with `-lglut -lGL -lGLU` when building programs that use this module.

**Important**: The library names should match what you'd pass to `-l`. For `libfoo.so`, use `"foo"`.

### Full Example: Color Module with Terminal Detection

Here's a complete example based on the `qd-color` module:

**src/terminal.c:**

```c
#include <qdrt/ffi.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

qd_exec_result is_terminal(qd_context* ctx) {
    qd_stack_push_int(ctx->st, isatty(STDOUT_FILENO));
    return (qd_exec_result){0};
}

qd_exec_result supports_color(qd_context* ctx) {
    if (!isatty(STDOUT_FILENO)) {
        qd_stack_push_int(ctx->st, 0);
        return (qd_exec_result){0};
    }

    const char* term = getenv("TERM");
    if (!term || strcmp(term, "dumb") == 0) {
        qd_stack_push_int(ctx->st, 0);
        return (qd_exec_result){0};
    }

    qd_stack_push_int(ctx->st, 1);
    return (qd_exec_result){0};
}

qd_exec_result no_color_set(qd_context* ctx) {
    qd_stack_push_int(ctx->st, getenv("NO_COLOR") != NULL);
    return (qd_exec_result){0};
}
```

**module.qd:**

```qd
// Terminal color output module with smart terminal detection

import "libcolor_static.a" as "term" {
    fn is_terminal( -- result:i64)
    fn supports_color( -- result:i64)
    fn no_color_set( -- result:i64)
}

// Check if colors should be used
fn should_use_color( -- enabled:i64) {
    term::no_color_set if {
        0 return
    }
    term::supports_color
}

// Helper to output color code only if colors are enabled
fn emit_if_color(code:str -- ) {
    should_use_color if {
        code print
    } else {
        drop
    }
}

pub fn red( -- ) {
    "\e[31m" emit_if_color
}

pub fn green( -- ) {
    "\e[32m" emit_if_color
}

pub fn reset( -- ) {
    "\e[0m" emit_if_color
}
```

## Using Dependencies

If your module depends on other modules, declare them in `quadrate.toml`:

```toml
[package]
name = "mymodule"
version = "1.0.0"
description = "My module"
license = "MIT"

[dependencies]
str = "*"
fmt = "*"
```

Then import them in your module:

```qd
use str
use fmt

pub fn greet(name:str -- ) {
    "Hello, " name str::concat "!" str::concat print nl
}
```

## Best Practices

1. **Use `pub` sparingly** - Only export functions that are part of your public API

2. **Document public functions** - Add `///` doc comments for each public function

3. **Include examples** - Help users understand how to use your module

4. **Version your releases** - Use git tags for versioning: `git tag v1.0.0`

5. **Follow naming conventions**:
   - Module names: lowercase, short (e.g., `color`, `mymath`)
   - Function names: snake_case (e.g., `is_even`, `to_upper`)

6. **Test before publishing** - Run your examples to ensure everything works

7. **Keep C code minimal** - Only use FFI when necessary for performance or system access

8. **Handle errors gracefully** - Use fallible functions (`!`) for operations that can fail

## Publishing Checklist

Before publishing your module:

- [ ] All public functions have doc comments
- [ ] `quadrate.toml` has correct name, version, description
- [ ] `README.md` explains installation and usage
- [ ] Examples in `examples/` directory work
- [ ] LICENSE file included
- [ ] Tested locally with `quadc -I . -r examples/simple.qd`
- [ ] If using FFI: C code builds with `quadpm build`
- [ ] Git repository created and pushed

## Summary

Creating a distributable module involves:

1. **Create `module.qd`** with `pub` functions
2. **Create `quadrate.toml`** package manifest
3. **Add documentation** (README.md, LICENSE)
4. **Include examples** for users
5. **Optionally add C code** in `src/` for FFI
6. **Publish** to a Git repository

Users install your module with:

```bash
quadpm get https://your-git-host/your-module
```

## See Also

- [Modules](learn/3-functions/modules.md) - Basic module usage
- [FFI](learn/7-advanced/ffi.md) - C interoperability details
- [Toolchain](toolchain.md) - quadpm and other tools
