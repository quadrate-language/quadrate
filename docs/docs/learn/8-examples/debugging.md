# Example: debugging

Techniques for debugging Quadrate programs.

## Stack debugging

`dump` prints the top value with its type and pops it. `dup dump` looks at it without
taking it:

```qd
fn main() {
	1 2 3
	dup dump    // int:3
	drop drop drop
}
```

To see the whole stack, try the code at the `quadrepl` prompt, where `stack` shows every value.

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

Use the runtime debug helper to print the full stack:

```
(gdb) call (void)qd_debug_print_stack(ctx)
(gdb) print qd_context_stack_size(ctx)
```

For low-level inspection, the internal stack structure is also accessible:

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
