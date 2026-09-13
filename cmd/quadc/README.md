# quadc

Quadrate compiler.

Compiles `.qd` source files to native executables via LLVM.

## Usage

```bash
quadc [options] <file>...
quadc [options] <file> -- [args]   # pass args to the program, with -r
quadc [options] -                  # read the source from stdin
```

## Options

| Option | Description |
|--------|-------------|
| `-o, --output <name>` | Output executable name (default: the source file's name) |
| `-O0`…`-O3` | Set optimization level (default: `-O0`) |
| `-g, --debug` | Generate debug information for GDB/LLDB |
| `-s, --stack-size <size>` | Set stack size (default: 1024) |
| `-I, --include <path>` | Add a module search path (repeatable) |
| `-l, --module <mod@ver>` | Pin a module to a version |
| `-r, --run` | Compile and run immediately (uses the JIT by default) |
| `--no-jit` | Link and execute instead of using the JIT, with `-r` |
| `--test` | Compile and run tests |
| `--coverage` | Print a function coverage report (use with `--test`) |
| `--target <triple>` | Cross-compile for a target |
| `--freestanding` | No hosted runtime — emit `.o` with no libc and no auto-main |
| `--werror` | Treat warnings as errors |
| `--verbose`, `--save-temps` | Show compilation steps; keep temporary files |
| `--dump-tokens`, `--dump-ast`, `--dump-ir` | Print the token stream, AST or LLVM IR |

## Examples

```bash
quadc main.qd                # Compile to an executable named 'main'
quadc -o prog main.qd        # Compile to an executable named 'prog'
quadc -r main.qd             # Compile and run immediately
quadc -r greet.qd -- Alice   # Compile and run with the argument 'Alice'
quadc -r -                   # Compile and run source piped on stdin
```

`quadc --help` is the authoritative option list. Every Quadrate tool accepts
`-h`/`--help`, `-v`/`--version` and `--no-color`.
