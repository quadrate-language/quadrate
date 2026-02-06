# Example: debugging

Techniques for debugging Quadrate programs.

## Stack debugging

Use the built-in debug instructions to inspect stack state:

```qd
fn main() {
	1 2 3
	prints    // 1 2 3 (print entire stack, non-destructive)
	printsv   // int:1 int:2 int:3 (with type info)
	printv    // int:3 (print and pop top value)
	prints    // 1 2
}
```

| Instruction | Pops | Type info | Description |
|-------------|------|-----------|-------------|
| `prints` | No | No | Print entire stack |
| `printsv` | No | Yes | Print entire stack with types |
| `printv` | Yes (one) | Yes | Print and pop top value |

## GDB debugging

Quadrate supports source-level debugging with GDB. Compile with `-g` to include debug symbols:

```bash
quadc -g -o myprogram myprogram.qd
gdb ./myprogram
```

### Setting breakpoints

Break at a specific line in your `.qd` source:

```
(gdb) break myprogram.qd:10
(gdb) run
```

### Inspecting the stack

The runtime stack is accessible through the context variable:

```
(gdb) print ctx->st->size
(gdb) print ctx->st->data[0]
```

### Stepping through code

Standard GDB commands work with Quadrate source files:

```
(gdb) next       # Step over
(gdb) step       # Step into
(gdb) continue   # Resume execution
(gdb) list       # Show source around current line
```

## Compiler diagnostics

### Dump tokens

See how the lexer tokenizes your source:

```bash
quadc --dump-tokens myprogram.qd
```

### Dump AST

See the parsed abstract syntax tree:

```bash
quadc --dump-ast myprogram.qd
```

### Dump LLVM IR

Inspect the generated LLVM intermediate representation:

```bash
quadc --dump-ir myprogram.qd
```

### Verbose compilation

Show detailed compilation information:

```bash
quadc --verbose myprogram.qd
```
