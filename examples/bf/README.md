# Brainfuck Interpreter

A complete Brainfuck interpreter implemented in Quadrate.

## Run

```bash
quad run bf.qd -- hello.bf
```

## Features

- Memory tape with 30,000 cells
- All 8 Brainfuck instructions: `>` `<` `+` `-` `.` `,` `[` `]`
- File I/O for loading programs
- Manual memory management with `mem::alloc` and `mem::free`
- `defer` for cleanup
