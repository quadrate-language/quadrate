# LLVM Backend for Quadrate

Alternative code generator that emits LLVM IR instead of transpiling to C.

## Architecture

```
.qd source → Parser (lib/qc) → AST → LLVM Generator → LLVM IR → Native Binary
```

## Features

✅ **Implemented:**
- Integer, float, and string literals
- Basic stack operations (prints, nl)
- Runtime function calls (dynamic dispatch for instructions)
- Main function generation
- LLVM IR output (.ll files)
- Object file generation (.o files)
- Executable linking with libqdrt

🚧 **TODO:**
- Control flow (if/else, loops, switch)
- User-defined functions (beyond main)
- Function calls and stack signatures
- Defer blocks
- Module system imports
- Optimizations (LLVM passes)

## Usage

### Build

```bash
# LLVM backend is built automatically if LLVM is detected
make debug
```

### Compile Quadrate Programs

```bash
# Using the LLVM test compiler
build/debug/bin/quadc-llvm/quadc-llvm input.qd output

# This generates:
# - output.ll   (LLVM IR - human readable)
# - output      (executable)
```

### Example

```quadrate
fn main( -- ) {
    "Hello from LLVM!" prints nl
    42 prints nl
    3.14 prints nl
}
```

```bash
build/debug/bin/quadc-llvm/quadc-llvm test.qd hello_llvm
./hello_llvm
```

Output:
```
"Hello from LLVM!"
"Hello from LLVM!" 42
"Hello from LLVM!" 42 3.14
```

## Implementation Details

### Runtime Integration

The LLVM backend links with the same `libqdrt` runtime as the C backend:

- `qd_create_context()` - Initialize stack context
- `qd_push_i/f/s()` - Push values onto stack
- `qd_prints()` - Print stack (non-destructive)
- `qd_nl()` - Print newline
- Runtime functions dynamically declared as needed

### Code Generation

1. **AST Traversal** - Walks the Quadrate AST using IAstNode interface
2. **IR Building** - Uses LLVM IRBuilder to emit LLVM instructions
3. **Type Mapping:**
   - Quadrate int → LLVM i32
   - Quadrate float → LLVM double
   - Quadrate string → LLVM i8* (global constants)
   - qd_context* → LLVM ptr (opaque)

4. **Function Generation:**
   - Creates LLVM `@main` function
   - Allocates Quadrate context on entry
   - Translates instructions to runtime calls
   - Frees context and returns 0

### LLVM IR Example

For `42 prints nl`:

```llvm
%ctx = call ptr @qd_create_context(i64 1024)
%1 = call %qd_exec_result @qd_push_i(ptr %ctx, i32 42)
%2 = call %qd_exec_result @qd_prints(ptr %ctx)
%3 = call %qd_exec_result @qd_nl(ptr %ctx)
call void @qd_free_context(ptr %ctx)
ret i32 0
```

## Advantages over C Backend

1. **Faster compilation** - No intermediate C generation
2. **Better optimization** - Direct access to LLVM optimization passes
3. **Cleaner IR** - Human-readable intermediate representation
4. **Potential for JIT** - Could enable REPL/interactive mode
5. **Better debug info** - Direct DWARF generation capability

## Dependencies

- LLVM 14+ (tested with LLVM 21.1.5)
- llvm-config tool
- Clang (for final linking step)

## Files

- `include/llvmgen/generator.h` - Public API
- `src/generator.cc` - LLVM IR generator implementation (~330 lines)
- `meson.build` - Build configuration

## Future Work

- Implement control flow structures
- Add function call support
- Integrate LLVM optimization passes
- Support for all Quadrate features
- Consider replacing C backend entirely
- Add JIT compilation mode for REPL
