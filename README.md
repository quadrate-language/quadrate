> **Canonical repository is hosted on SourceHut:** https://git.sr.ht/~klahr/quadrate  
> Issues and patches are accepted on both SourceHut and GitHub.

# Quadrate

A stack-based programming language that compiles to native code.

**Documentation**: https://quad.r8.rs

## What is Quadrate?

Quadrate is a Forth-inspired stack language with modern features: static typing, modules, and native code generation via LLVM. Values live on an explicit stack, and functions manipulate that stack directly.

Quadrate can be used as:
- **Standalone language**: Compile `.qd` files to native executables
- **Embedded scripting**: JIT-compile Quadrate code in C/C++ applications at runtime
- **Interactive REPL**: Experiment with stack-based programming interactively

## Quick Start

Install:
```bash
git clone https://git.sr.ht/~klahr/quadrate
cd quadrate
make && sudo make install
```

Hello World (`hello.qd`):
```rust
fn main( -- ) {
    "Hello, World!" .  nl
}
```

Compile and run:
```bash
quadc hello.qd -r
```

## Examples

### Simple Math
```rust
fn main( -- ) {
    5 3 +       // Push 5, push 3, add them
    . nl        // Print result (8) and newline
}
```

### Function with Stack Signature
```rust
fn square(x:f64 -- result:f64) {
    dup *       // Duplicate x, multiply
}

fn main( -- ) {
    7.0 square . nl    // Prints 49.0
}
```

### Control Flow
```rust
fn main( -- ) {
    10 5 > if {
        "10 is greater" . nl
    } else {
        "5 is greater" . nl
    }
}
```

### Loops
```rust
fn main( -- ) {
    0 10 1 for {
        $ . nl    // $ is the loop counter
    }
}
```

## Core Concepts

**Stack Operations**: `dup` (duplicate), `swap`, `drop`, `over`, `rot`

**Arithmetic**: `+ - * / %` `sqrt` `abs` `inc` `dec`

**Comparison**: `< > <= >= == !=`

**Control Flow**: `if`/`else`, `for` loops, `switch`/`case`

**Functions**: Can have stack signatures showing inputs and outputs
```rust
fn add(a:f64 b:f64 -- sum:f64) {
    +
}
```

## Embedding & Scripting

Quadrate can be embedded as a JIT-compiled scripting language in C/C++ applications. The embedding API provides runtime compilation and execution of Quadrate code with native performance.

### Quick Example

```cpp
#include <qd/qd.h>

int main() {
    // Create runtime context
    qd_context* ctx = qd_create_context(1024);

    // Create a module and add Quadrate code
    qd_module* math = qd_get_module(ctx, "math");
    qd_add_script(math, "fn double(x:i64 -- result:i64) { 2 * }");

    // JIT compile to native code
    qd_build(math);

    // Execute: pushes 5, calls double, prints result (10)
    qd_execute(ctx, "5 math::double . nl");

    qd_free_context(ctx);
    return 0;
}
```

### Features

- **JIT Compilation**: Scripts compile to native code via LLVM at runtime
- **Multiple Modules**: Organize code into independent modules
- **Native Functions**: Register C/C++ functions callable from Quadrate
- **Incremental Building**: Add scripts incrementally before compilation
- **Zero-copy Integration**: Direct access to runtime stack

### Registering Native Functions

```cpp
#include <qd/qd.h>
#include <time.h>

// Native function callable from Quadrate
qd_exec_result get_timestamp(qd_context* ctx) {
    time_t now = time(nullptr);
    return qd_push_i(ctx, static_cast<int64_t>(now));
}

int main() {
    qd_context* ctx = qd_create_context(1024);
    qd_module* utils = qd_get_module(ctx, "utils");

    // Register C function
    qd_register_function(utils, "get_timestamp",
        reinterpret_cast<void(*)()>(get_timestamp));

    // Add Quadrate wrapper
    qd_add_script(utils, "fn show_time( -- ) { get_timestamp . nl }");
    qd_build(utils);

    // Call from Quadrate
    qd_execute(ctx, "utils::show_time");

    qd_free_context(ctx);
    return 0;
}
```

### Compile & Link

```bash
# Compile
clang++ myapp.cc -o myapp -lqd -lqdrt

# Run with library path
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH
./myapp
```

See `examples/embed/` for complete examples including:
- Basic JIT compilation
- Multiple module management
- Native function registration
- Incremental script building

## Standard Library

Import with `use`:
```rust
use fmt

fn main( -- ) {
    "Hello %s!\n" "World" fmt::printf
}
```

Available modules:
- **fmt** - Formatted printing (`printf`, `sprintf`)
- **io** - File I/O operations
- **net** - TCP networking
- **time** - Sleep and timing functions
- **str** - String manipulation
- **math** - Mathematical functions
- **os** - Operating system interface
- **mem** - Memory operations
- **bits** - Bit manipulation
- **base64** - Base64 encoding/decoding

## Build Commands

```bash
make           # Build debug version
make release   # Build optimized version
make tests     # Run all tests (includes embed tests)
make valgrind  # Run tests with memory leak checking
make examples  # Build example programs
make format    # Format source code with clang-format
make clean     # Remove all build artifacts
```

## Tools

- **quadc** - Compiler (`-r` to run, `-o name` for output, `-g` for debug info)
- **quadfmt** - Code formatter
- **quadlsp** - Language server for IDE integration
- **quadrate** - Interactive REPL for quick experimentation
- **quaduses** - Show module dependencies and usage
- **quadpm** - Package manager for installing and managing Quadrate packages
- **libqd** - Embedding library for JIT compilation in C/C++ applications
- **libqdrt** - Runtime library providing stack operations and built-ins

## Interactive REPL

Experiment with Quadrate interactively:

```bash
$ quadrate
Quadrate REPL v0.1.0
Type 'help' for commands, 'exit' to quit

[]> 5 3
[5 3]> +
[8]> dup
[8 8]> *
[64]> clear
[]>
```

REPL commands:
- `clear` - Clear the stack
- `help` - Show available commands
- `exit` or `Ctrl-D` - Exit REPL

## Debugging

Quadrate programs can be debugged with GDB or LLDB:

```bash
# Compile with debug information
quadc myprogram.qd -g -o myprogram

# Debug with GDB
gdb ./myprogram
```

Inside GDB:
```gdb
(gdb) break 5              # Break at line 5
(gdb) run
(gdb) print ctx->st->size  # Check stack size
```

The `-g` flag generates DWARF debug info with:
- Source-level debugging (step through `.qd` files)
- Breakpoints on line numbers and functions
- Stack inspection via `ctx` variable
- Full integration with IDE debuggers

## Installation

Default install to `/usr`:
```bash
sudo make install
```

Custom prefix:
```bash
sudo make install PREFIX=/usr/local
```

Uninstall:
```bash
sudo make uninstall
```

## Dependencies

Build dependencies:
- Meson build system
- libu8t (UTF-8 tokenizer)
- LLVM (for code generation)

For embedding (linking against libqd):
- libqd (JIT compilation and module management)
- libqdrt (runtime and stack operations)
- LLVM libraries (dynamically loaded at runtime)

## License

Quadrate is licensed under the **GNU General Public License v3**.  
See [`LICENSE`](./LICENSE) for full terms.

### Note on AI assistance

Some parts of this codebase were originally drafted with assistance from an AI model
and then reviewed and modified by a human maintainer.

## Maintainers

[~klahr](https://sr.ht/~klahr)

## Contributing

We enthusiastically encourage and welcome contributions.

### Reporting Issues

Report bugs and feature requests at: https://todo.sr.ht/~klahr/quadrate

### Submitting Patches

Send patches to: ~klahr/quadrate@lists.sr.ht

Learn more about sourcehut workflows: https://man.sr.ht/git.sr.ht/send-email.md
