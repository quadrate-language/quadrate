# Quadrate

> **⚠️ Alpha Status**: Quadrate is under active development. APIs may change, and some features are incomplete. Not recommended for production use yet.

> **Canonical repository:** https://git.sr.ht/~klahr/quadrate
> Issues and patches welcome on both SourceHut and GitHub.

**A modern stack-based language that compiles to native code.**

Quadrate combines the elegance of Forth's stack-oriented programming with contemporary features like static typing, modules, and LLVM-powered native compilation. Whether you need a standalone systems language or an embedded scripting engine, Quadrate compiles to native machine code.

**Documentation**: https://quad.r8.rs

---

## Why Quadrate?

**🚀 Native Compilation**
Compiles directly to native machine code via LLVM. No VM overhead, no garbage collection pauses—predictable execution with LLVM optimizations.

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

## Quick start

### Installation

```bash
git clone https://git.sr.ht/~klahr/quadrate
cd quadrate
make release && sudo make install
```

**Build Requirements**:
- Meson build system
- C11 and C++20 compiler (GCC or Clang)
- LLVM ≥14.0 (for JIT/embedding features only)

All other dependencies are automatically downloaded and built as subprojects.

### Hello World

Create `hello.qd`:
```rust
fn main() {
    "Hello, World!" print nl
}
```

Compile and run:
```bash
quadc -r hello.qd
```

That's it! The `-r` flag compiles and immediately runs your program.

---

## Language overview

### Stack-oriented programming

Values live on an explicit stack. Functions manipulate the stack directly:

```rust
fn main() {
    5 3 +       // Stack: [5] [3] → [8]
    print nl        // Print 8 and newline
}
```

### Type-safe stack signatures

Functions declare their stack effects using type signatures:

```rust
use math

fn square(x:f64 -- result:f64) {
    math::sq
}

fn distance(x1:f64 y1:f64 x2:f64 y2:f64 -- dist:f64) {
    // (x2-x1)² + (y2-y1)²
    swap rot - math::sq   // Calculate (x2-x1)²
    rot rot - math::sq    // Calculate (y2-y1)²
    + math::sqrt          // Add and take square root
}

fn main() {
    7.0 square print nl           // 49.0
    0.0 0.0 3.0 4.0 distance print nl  // 5.0
}
```

The signature `(x:f64 y:f64 -- result:f64)` means: "Consumes two f64 values from the stack, produces one f64 result."

### Built-in stack operations

| Operation | Effect | Description |
|-----------|--------|-------------|
| `dup` | `(a -- a a)` | Duplicate top value |
| `drop` | `(a -- )` | Discard top value |
| `swap` | `(a b -- b a)` | Swap top two values |
| `over` | `(a b -- a b a)` | Copy second value to top |
| `rot` | `(a b c -- b c a)` | Rotate top three values |

### Control flow

```rust
fn main() {
    // Conditionals
    10 5 > if {
        "10 is greater" print nl
    } else {
        "5 is greater" print nl
    }

    // For loops (start end step)
    0 10 1 for i {
        i print nl    // i is the loop counter
    }

    // While loops
    0
    loop {
        dup print nl
        inc
        dup 10 >= if { break }
    }
    drop

    // Pattern matching
    42 switch {
        0  { "zero" print nl }
        42 { "answer" print nl }
        _  { "other" print nl }
    }
}
```

### Modules and code organization

```rust
use math
use fmt

fn hypotenuse(a:f64 b:f64 -- c:f64) {
    math::sq swap math::sq + math::sqrt
}

fn main() {
    3.0 4.0 hypotenuse
    "Hypotenuse: %f\n" fmt::printf
}
```

---

## Standard library

Quadrate includes batteries for common tasks:

### Core modules

| Module | Purpose | Key Functions |
|--------|---------|---------------|
| **fmt** | Formatted I/O | `printf`, `sprintf`, `scanf` |
| **io** | File operations | `open`, `read`, `write`, `close`, `read_file` |
| **mem** | Memory management | `alloc`, `free`, `copy`, `zero`, `get_byte`, `set_byte` |
| **str** | String manipulation | `concat`, `split`, `substr`, `replace`, `len` |
| **math** | Math and linear algebra | `sin`, `cos`, `sqrt`, `Vec2`, `Vec3`, `Mat4`, `Quat` |
| **bits** | Bit manipulation | `and`, `or`, `xor`, `lshift`, `rshift` |
| **limits** | Integer limits | `MaxInt`, `MinInt`, `MaxUint` |

### System & concurrency

| Module | Purpose | Key Functions |
|--------|---------|---------------|
| **os** | System interface | `env`, `exec`, `getpid`, `getcwd`, `exit` |
| **time** | Time & sleep | `now`, `sleep`, `Second`, `Millisecond` |
| **thread** | Threading | `spawn`, `join`, `detach`, `Mutex`, `Channel`, `WaitGroup` |
| **signal** | Signal handling | `trap`, `pending`, `clear`, `SigInt`, `SigTerm` |

### Utility modules

| Module | Purpose | Key Functions |
|--------|---------|---------------|
| **rand** | Random numbers | `new`, `int`, `range`, `bool`, `with_seed` |
| **path** | File path manipulation | `dirname`, `basename`, `ext`, `join`, `is_absolute` |
| **sb** | String builder | `new`, `append`, `append_char`, `build`, `clear` |
| **bytes** | Byte arrays | `new`, `get`, `set`, `len`, `to_str` |
| **strconv** | String conversion | `itoa`, `atoi`, `ftoa`, `atof` |
| **unicode** | Unicode support | `is_letter`, `is_digit`, `to_upper`, `to_lower` |
| **flag** | CLI argument parsing | `string`, `int`, `bool`, `parse` |
| **testing** | Unit testing | `assert_eq`, `assert_ne`, `assert_true`, `fail` |

### External modules

These modules are available as separate packages via `quadpm`:

| Module | Purpose | Repository |
|--------|---------|------------|
| **base64** | Base64 encoding | [quadrate-language/base64](https://github.com/quadrate-language/base64) |
| **compress** | Compression | [quadrate-language/compress](https://github.com/quadrate-language/compress) |
| **crypto** | SHA-256, CRC32, etc. | [quadrate-language/crypto](https://github.com/quadrate-language/crypto) |
| **ct** | Compile-time execution | [quadrate-language/ct](https://github.com/quadrate-language/ct) |
| **hex** | Hex encoding | [quadrate-language/hex](https://github.com/quadrate-language/hex) |
| **hof** | Higher-order functions | [quadrate-language/hof](https://github.com/quadrate-language/hof) |
| **http** | HTTP client/server | [quadrate-language/http](https://github.com/quadrate-language/http) |
| **json** | JSON parsing | [quadrate-language/json](https://github.com/quadrate-language/json) |
| **log** | Logging | [quadrate-language/log](https://github.com/quadrate-language/log) |
| **net** | TCP networking | [quadrate-language/net](https://github.com/quadrate-language/net) |
| **regex** | Regular expressions | [quadrate-language/regex](https://github.com/quadrate-language/regex) |
| **sort** | Array sorting | [quadrate-language/sort](https://github.com/quadrate-language/sort) |
| **sqlite** | SQLite database | [quadrate-language/sqlite](https://github.com/quadrate-language/sqlite) |
| **tls** | TLS/SSL | [quadrate-language/tls](https://github.com/quadrate-language/tls) |
| **uri** | URI parsing/building | [quadrate-language/uri](https://github.com/quadrate-language/uri) |
| **uuid** | UUID generation | [quadrate-language/uuid](https://github.com/quadrate-language/uuid) |

Example with formatted output:
```rust
use fmt
use time

fn main() {
    time::now "Current time: %d\n" fmt::printf
}
```

### Example: UUID and random numbers

```rust
use uuid
use rand

fn main() {
    // Generate random UUID
    uuid::v4 print nl

    // Random number generation
    rand::new -> rng
    rng 1 100 rand::range -> rng -> n
    "Random 1-100: " print n print nl
}
```

### Example: path manipulation

```rust
use path

fn main() {
    "/home/user/docs/report.pdf" -> p
    p path::dirname print nl   // /home/user/docs
    p path::basename print nl  // report.pdf
    p path::ext print nl       // .pdf
}
```

---

## Embedding in C/C++

Quadrate's JIT engine lets you embed scripting in your applications. Scripts compile to native code at runtime via LLVM—no interpreter overhead.

### Basic embedding

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
    qd_execute(ctx, "5 math::double print nl");

    qd_free_context(ctx);
    return 0;
}
```

### Registering native functions

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
    qd_register_function(utils, "get_timestamp", reinterpret_cast<void(*)()>(get_timestamp));

    qd_build(utils);

    // Call native Quadrate function, and print the result
    qd_execute(ctx, "utils::get_timestamp print nl");

    qd_free_context(ctx);
    return 0;
}
```

### Building with libqd

```bash
# Compile and link
clang++ myapp.cc -o myapp -lqd -lqdrt

# Run
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

## Interactive development

### REPL

Experiment with Quadrate interactively:

```bash
$ quad repl
# or: quadrepl

[]> 5 3
[5 3]> +
[8]> dup
[8 8]> *
[64]> print
64
[]>
```

**REPL Commands:**
- `clear` - Clear the stack
- `Ctrl-D` - Exit REPL

### Code formatter

Keep your code consistent:

```bash
quad fmt myfile.qd
```

```bash
quadfmt myfile.qd          # Preview formatting
quadfmt -w myfile.qd       # Format in place
```

### Language server

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
| **quad** | Unified CLI (`quad build`, `quad run`, `quad test`, `quad fmt`, etc.) |
| **quadc** | Compiler (`-r` run, `-o` output, `-g` debug info, `--save-temps` keep intermediate files) |
| **quadfmt** | Code formatter with consistent style |
| **quadlint** | Static analyzer for common issues |
| **quadlsp** | Language server for IDE integration |
| **quadrepl** | Interactive REPL for quick experimentation |
| **quaduses** | Analyze module dependencies and function usage |
| **quadpm** | Package manager for installing and managing libraries |

---

## Building from source

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

# Install
make install

# Uninstall
make uninstall

# Clean build artifacts
make clean
```

### Build options

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
quad build -g -o myprogram myprogram.qd 

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

## Testing

Quadrate includes a built-in test framework for writing and running unit tests.

### Writing tests

Use the `test` keyword to define test blocks, and assertion functions from the `testing` module:

```rust
use testing

test "basic arithmetic" {
    2 3 + 5 testing::assert_eq
    10 5 - 5 testing::assert_eq
}

test "string comparison" {
    "hello" "hello" testing::assert_eq
    "foo" "bar" testing::assert_ne
}

test "boolean checks" {
    1 testing::assert_true
    0 testing::assert_false
    "non-empty" testing::assert_true
    "" testing::assert_false
}
```

### Available assertions

| Function | Description |
|----------|-------------|
| `testing::assert_eq` | Assert two values are equal (works with any type) |
| `testing::assert_ne` | Assert two values are not equal |
| `testing::assert_true` | Assert value is truthy (non-zero, non-empty) |
| `testing::assert_false` | Assert value is falsy (zero, empty, null) |
| `testing::fail` | Unconditionally fail with a message |

All assertions are polymorphic—they work with integers, floats, strings, and pointers.

### Running tests

```bash
# Compile and run tests
quad test myfile_test.qd
```

### Example output

```
Running 3 tests...
  ✓ basic_arithmetic
  ✓ string_comparison
  ✗ intentional_failure

2 passed, 1 failed
```

Tests return exit code 0 if all pass, 1 if any fail—making them suitable for CI pipelines.

### Test with failure messages

When an assertion fails, it prints detailed information:

```
Assertion failed: assert_eq
  expected: 5 (i64)
       got: 10 (i64)

Stack trace:
    0: myfile.qd::test(basic arithmetic)
```

---

## Real-world examples

### File processing

```rust
use io
use mem
use fmt

fn process_file(filename:str -- ) {
    // Open file for reading
    io::Read io::open! -> file

    // Allocate buffer for reading
    1024 mem::alloc! -> buffer

    // Read and process data
    file buffer 1024 io::read! -> bytes_read

    bytes_read "Read %d bytes\n" fmt::printf

    // Cleanup
    buffer mem::free
    file io::close!
}

fn main() {
    "input.txt" process_file
}
```

### Game loop

```rust
use time

fn update() {
    // Update game state
}

fn render() {
    // Render frame
}

fn main() {
    time::Millisecond 16 * -> frame_time  // 60 FPS = 16.67ms

    loop {
        time::now -> start_time

        update
        render

        time::now start_time - -> elapsed
        frame_time elapsed > if {
            frame_time elapsed - time::sleep
        }
    }
}
```

---

## Editor support

- **Neovim**: https://git.sr.ht/~klahr/quadrate.nvim
- **VS Code**: https://git.sr.ht/~klahr/quadrate-vscode

---

## Platform support

**Tested:**
- Linux x86_64 (Arch Linux, Debian, Haiku)

**Potentially Compatible:**
- Linux aarch64, FreeBSD, other Unix-like systems

Contributions for testing on other platforms are welcome.

**Compiler Requirements:**
- C11 and C++20 compiler (GCC 10+ or Clang 10+)
- LLVM 14+

---

## Contributing

We enthusiastically welcome contributions!

### Reporting issues

Report bugs and request features:
**SourceHut**: https://todo.sr.ht/~klahr/quadrate
**GitHub**: https://github.com/quadrate-lang/quadrate/issues

### Submitting patches

**Email patches to**: ~klahr/quadrate@lists.sr.ht

New to sourcehut's email workflow?
See: https://man.sr.ht/git.sr.ht/send-email.md

**Or use GitHub**: Submit pull requests if you prefer

### Development setup

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

### Note on AI assistance

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
