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

The runtime's stack structure is reachable through `ctx`:

```
(gdb) print ctx->st->size       # how many values are on the stack
(gdb) print ctx->st->data[0]    # the bottom value, with its type
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

The `--dump-*` flags print what they dump and then build the program as usual, so the
executable lands in the current directory. Pass `-o` to put it somewhere else.

### Dump tokens

See how the lexer tokenizes your source:

```bash
quadc --dump-tokens myprogram.qd -o /tmp/myprogram
```

### Dump AST

See the parsed abstract syntax tree:

```bash
quadc --dump-ast myprogram.qd -o /tmp/myprogram
```

### Dump LLVM IR

Inspect the generated LLVM intermediate representation:

```bash
quadc --dump-ir myprogram.qd -o /tmp/myprogram
```

### Verbose compilation

Show detailed compilation information:

```bash
quadc --verbose myprogram.qd
```
