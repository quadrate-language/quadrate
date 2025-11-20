# Quadrate

> **⚠️ Alpha Status**: Quadrate is under active development. APIs may change, and some features are incomplete. Not recommended for production use yet.

> **Canonical repository:** https://git.sr.ht/~klahr/quadrate
> Issues and patches welcome on both SourceHut and GitHub.

**A modern stack-based language that compiles to native code.**

Quadrate combines the elegance of Forth's stack-oriented programming with contemporary features like static typing, modules, and LLVM-powered native compilation. Whether you need a standalone systems language or a high-performance embedded scripting engine, Quadrate delivers native speed with minimal overhead.

**Documentation**: https://quad.r8.rs

---

## Why Quadrate?

**🚀 Native Performance**
Compiles directly to native machine code via LLVM. No VM overhead, no garbage collection pauses—just fast, predictable execution.

**⚡ Dual-Mode Operation**
- **Standalone**: Compile `.qd` files to executables like C/C++
- **Embedded**: JIT-compile scripts at runtime in your C/C++ applications

**📦 Zero-Dependency Binaries**
Statically linked executables with no runtime dependencies. Ship a single binary that runs anywhere.

**🔧 Systems Programming**
Direct memory access, pointer manipulation, and FFI with C libraries. Build anything from CLI tools to game engines.

**🎯 Explicit and Predictable**
Stack-based semantics make data flow obvious. No hidden allocations, no implicit conversions—you control everything.

---

## Quick Start

### Installation

```bash
git clone https://git.sr.ht/~klahr/quadrate
cd quadrate
make release && sudo make install
```

**Dependencies**: Meson, LLVM, libu8t (UTF-8 tokenizer)

### Hello World

Create `hello.qd`:
```rust
fn main( -- ) {
    "Hello, World!" . nl
}
```

Compile and run:
```bash
quadc -r hello.qd
```

That's it! The `-r` flag compiles and immediately runs your program.

---

## Language Overview

### Stack-Oriented Programming

Values live on an explicit stack. Functions manipulate the stack directly:

```rust
fn main( -- ) {
    5 3 +       // Stack: [5] [3] → [8]
    . nl        // Print 8 and newline
}
```

### Type-Safe Stack Signatures

Functions declare their stack effects using type signatures:

```rust
fn square(x:f64 -- result:f64) {
    dup *       // Duplicate x, then multiply
}

fn distance(x1:f64 y1:f64 x2:f64 y2:f64 -- dist:f64) {
    // (x2-x1)² + (y2-y1)²
    swap rot - dup *      // Calculate (x2-x1)²
    rot rot - dup *       // Calculate (y2-y1)²
    + sqrt                // Add and take square root
}

fn main( -- ) {
    7.0 square . nl           // 49.0
    0.0 0.0 3.0 4.0 distance . nl  // 5.0
}
```

The signature `(x:f64 y:f64 -- result:f64)` means: "Consumes two f64 values from the stack, produces one f64 result."

### Built-in Stack Operations

| Operation | Effect | Description |
|-----------|--------|-------------|
| `dup` | `(a -- a a)` | Duplicate top value |
| `drop` | `(a -- )` | Discard top value |
| `swap` | `(a b -- b a)` | Swap top two values |
| `over` | `(a b -- a b a)` | Copy second value to top |
| `rot` | `(a b c -- b c a)` | Rotate top three values |

### Control Flow

```rust
fn main( -- ) {
    // Conditionals
    10 5 > if {
        "10 is greater" . nl
    } else {
        "5 is greater" . nl
    }

    // For loops (start end step)
    0 10 1 for {
        $ . nl    // $ is the loop counter
    }

    // While loops
    0
    loop {
        dup . nl
        inc
        dup 10 >= if { break }
    }
    drop

    // Pattern matching
    42 switch {
        case 0  { "zero" . nl }
        case 42 { "answer" . nl }
        default { "other" . nl }
    }
}
```

### Modules and Code Organization

```rust
use math
use fmt

fn hypotenuse(a:f64 b:f64 -- c:f64) {
    dup * swap dup * + math::sqrt
}

fn main( -- ) {
    3.0 4.0 hypotenuse
    "Hypotenuse: %.2f\n" swap fmt::printf
}
```

---

## Standard Library

Quadrate includes batteries for common tasks:

| Module | Purpose | Key Functions |
|--------|---------|---------------|
| **fmt** | Formatted I/O | `printf`, `sprintf`, `scanf` |
| **io** | File operations | `open`, `read`, `write`, `close` |
| **net** | TCP networking | `listen`, `accept`, `connect`, `send`, `recv` |
| **str** | String manipulation | `concat`, `split`, `substr`, `replace` |
| **math** | Mathematics | `sin`, `cos`, `sqrt`, `pow`, `log` |
| **time** | Time & sleep | `now`, `sleep`, `format_time` |
| **os** | System interface | `env`, `exec`, `getpid`, `getcwd` |
| **mem** | Memory ops | `alloc`, `free`, `copy`, `compare` |
| **bits** | Bit manipulation | `and`, `or`, `xor`, `shl`, `shr` |
| **base64** | Encoding | `encode`, `decode` |

Example with formatted output:
```rust
use fmt
use time

fn main( -- ) {
    "Current time: %d\n" time::now fmt::printf
}
```

---

## Embedding in C/C++

Quadrate's JIT engine lets you embed high-performance scripting in your applications. Scripts compile to native code at runtime via LLVM—no interpreter overhead.

### Basic Embedding

```cpp
#include <qd/qd.h>

int main() {
    // Create runtime context
    qd_context* ctx = qd_create_context(1024);

    // Create module and add Quadrate code
    qd_module* math = qd_get_module(ctx, "math");
    qd_add_script(math, "fn double(x:i64 -- result:i64) { 2 * }");

    // JIT compile to native code
    qd_build(math);

    // Execute (pushes 5, calls double, prints 10)
    qd_execute(ctx, "5 math::double . nl");

    qd_free_context(ctx);
    return 0;
}
```

### Registering Native Functions

Expose C/C++ functions to Quadrate scripts:

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

    // Use it from Quadrate
    qd_add_script(utils, "fn show_time( -- ) { get_timestamp . nl }");
    qd_build(utils);

    qd_execute(ctx, "utils::show_time");

    qd_free_context(ctx);
    return 0;
}
```

### Building with libqd

```bash
# Compile and link
clang++ myapp.cc -o myapp -lqd -lqdrt

# Run (ensure libraries are in path)
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH
./myapp
```

**Key Features:**
- **JIT Compilation**: Scripts compile to native code via LLVM
- **Multiple Modules**: Organize code, load modules dynamically
- **Two-Way FFI**: Call C from Quadrate and vice versa
- **Incremental Building**: Add scripts over time, compile once
- **Zero-Copy Stack Access**: Direct manipulation via low-level API

See `examples/embed/` for complete working examples.

---

## Interactive Development

### REPL

Experiment with Quadrate interactively:

```bash
$ quadrate
Quadrate REPL v0.1.0
Type 'help' for commands, 'exit' to quit

[]> 5 3
[5 3]> +
[8]> dup
[8 8]> *
[64]> .
64
[64]> clear
[]>
```

**REPL Commands:**
- `clear` - Clear the stack
- `help` - Show available commands
- `exit` / `Ctrl-D` - Exit REPL

### Code Formatter

Keep your code consistent:

```bash
quadfmt myfile.qd          # Preview formatting
quadfmt -w myfile.qd       # Format in place
```

### Language Server

IDE integration via LSP:

```bash
quadlsp
```

Supports:
- Syntax highlighting
- Auto-completion
- Go-to-definition
- Real-time diagnostics
- Function signature hints

---

## Toolchain

| Tool | Purpose |
|------|---------|
| **quadc** | Compiler (`-r` run, `-o` output, `-g` debug info, `--save-temps` keep intermediate files) |
| **quadfmt** | Code formatter with consistent style |
| **quadlsp** | Language server for IDE integration |
| **quadrate** | Interactive REPL for quick experimentation |
| **quaduses** | Analyze module dependencies and function usage |
| **quadpm** | Package manager for installing and managing libraries |

---

## Building from Source

```bash
# Clone repository
git clone https://git.sr.ht/~klahr/quadrate
cd quadrate

# Debug build (default)
make

# Release build (optimized)
make release

# Run test suite
make tests

# Run tests with valgrind (memory leak detection)
make valgrind

# Build example programs
make examples

# Format source code
make format

# Install to /usr (or PREFIX=/path/to/install)
sudo make install

# Uninstall
sudo make uninstall

# Clean build artifacts
make clean
```

### Build Options

```bash
# Custom build with Meson directly
meson setup build --buildtype=release -Dbuild_tests=true
meson compile -C build
meson test -C build --print-errorlogs
```

---

## Debugging

Quadrate generates full DWARF debug information:

```bash
# Compile with debug symbols
quadc myprogram.qd -g -o myprogram

# Debug with GDB
gdb ./myprogram
```

Inside GDB:
```gdb
(gdb) break main           # Break in main function
(gdb) break myfile.qd:42   # Break at line 42 in source
(gdb) run
(gdb) print ctx->st->size  # Inspect stack size
(gdb) print ctx->st->data[0]  # Examine stack values
```

**Debug Features:**
- Source-level debugging (step through `.qd` files line-by-line)
- Breakpoints on functions and line numbers
- Stack inspection via `ctx` variable
- Full integration with GDB, LLDB, and IDE debuggers

---

## Real-World Examples

### HTTP Server (with net module)

```rust
use net
use fmt

fn handle_client(client:i64 -- ) {
    dup "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello, World!"
    net::send drop
    net::close
}

fn main( -- ) {
    8080 net::listen
    loop {
        dup net::accept
        handle_client
    }
}
```

### File Processing

```rust
use io
use str
use fmt

fn count_lines(filename:str -- count:i64) {
    "r" io::open
    0 swap  // count file_handle
    loop {
        dup io::read_line  // count fd line
        dup 0 == if {
            drop drop break
        }
        drop swap inc swap  // Increment count
    }
    dup io::close drop
}

fn main( -- ) {
    "input.txt" count_lines
    "Lines: %d\n" swap fmt::printf
}
```

### Game Loop

```rust
use time

fn update(dt:f64 -- ) {
    // Update game state
}

fn render( -- ) {
    // Render frame
}

fn main( -- ) {
    0.016 // 60 FPS target (16ms)
    loop {
        time::now // start_time dt
        swap dup update
        render

        time::now swap - // frame_time dt
        dup rot < if {
            swap over - 1000.0 * i time::sleep
        } else {
            drop
        }
    }
}
```

---

## Performance

Quadrate compiles to native code with LLVM optimizations:

- **Zero overhead abstractions**: Stack operations compile to register moves
- **Static dispatch**: No virtual calls, no dynamic lookup
- **Aggressive inlining**: Small functions inline completely
- **LLVM backend**: Benefits from decades of optimization research
- **No GC pauses**: Manual memory management where you need it
- **Predictable performance**: No JIT warm-up, consistent timing

Benchmark example (calculating primes):
```
Quadrate:     ?.??s
C (gcc -O3):  ?.??s
Python:       ?.??s
JavaScript:   ?.??s
```

*(Results vary by workload. Stack-heavy code performs exceptionally well.)*

---

## Project Structure

```
quadrate/
├── cmd/          # Command-line tools (quadc, quadfmt, quadlsp, etc.)
├── lib/
│   ├── qc/       # Compiler frontend (parser, semantic analysis)
│   ├── cgen/     # C code generator (standalone compilation)
│   ├── llvmgen/  # LLVM IR generator (JIT compilation)
│   ├── qdrt/     # Low-level runtime (stack, builtins)
│   ├── qd/       # High-level embedding API
│   └── std*qd/   # Standard library modules
├── tests/        # Test suites
├── examples/     # Example programs
└── editors/      # Editor integrations (tree-sitter, LSP)
```

---

## Platform Support

**Primary Platform:**
- Linux (x86_64, aarch64) — tested in CI

**Potentially Compatible:**
- macOS (x86_64, Apple Silicon) — should work but untested
- BSD systems (FreeBSD, OpenBSD) — should work but untested

**Note**: Only Linux is regularly tested. Other Unix-like systems should work in theory (standard POSIX + LLVM), but have not been verified. Contributions for testing and fixes on other platforms are welcome!

**Compiler Backends:**
- GCC (standalone compilation)
- Clang (standalone compilation)
- LLVM JIT (embedded scripting)

---

## Contributing

We enthusiastically welcome contributions!

### Reporting Issues

Report bugs and request features:
**SourceHut**: https://todo.sr.ht/~klahr/quadrate
**GitHub**: https://github.com/quadrate-lang/quadrate/issues

### Submitting Patches

**Email patches to**: ~klahr/quadrate@lists.sr.ht

New to sourcehut's email workflow?
See: https://man.sr.ht/git.sr.ht/send-email.md

**Or use GitHub**: Submit pull requests if you prefer

### Development Setup

```bash
git clone https://git.sr.ht/~klahr/quadrate
cd quadrate
make              # Build debug version
make tests        # Run test suite
make format       # Format code before committing
```

---

## License

**GNU General Public License v3.0**

Quadrate is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

See [`LICENSE`](./LICENSE) for full terms.

### Note on AI Assistance

Portions of this codebase were initially drafted with AI assistance and subsequently reviewed, modified, and maintained by humans. All code meets the same quality standards regardless of origin.

---

## Maintainer

**~klahr** — https://sr.ht/~klahr

---

## Resources

- **Documentation**: https://quad.r8.rs
- **Repository**: https://git.sr.ht/~klahr/quadrate
- **Issue Tracker**: https://todo.sr.ht/~klahr/quadrate
- **Mailing List**: ~klahr/quadrate@lists.sr.ht

---
