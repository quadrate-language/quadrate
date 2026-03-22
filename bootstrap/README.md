# Quadrate Bootstrap

This directory contains the bootstrap artifact for the self-hosted Quadrate compiler.

## How It Works

`quadc.ll` is a portable LLVM IR file that contains the self-hosted compiler. Any machine with `clang` and the Quadrate runtime libraries can compile it into a native binary:

```bash
clang bootstrap/quadc.ll -Ldist/lib -lqdrt -lm -lstdc++ \
  -lqdio -lqdos -lqdstr -lqdstrconv -lqdmem -o quadc
```

This native `quadc` can then compile Quadrate source files, including the compiler's own source.

## 3-Stage Bootstrap Verification

```bash
make bootstrap
```

This runs:

1. **Stage 0**: Compile `quadc.ll` → native `quadc-stage0`
2. **Stage 1**: `quadc-stage0` compiles `quadc.qd` → generates `stage1.ll`
3. **Stage 2**: Compile `stage1.ll` → `quadc-stage1`, which compiles `quadc.qd` → `stage2.ll`
4. **Verify**: `stage1.ll` == `stage2.ll` (fixed-point check)

If stage 1 and stage 2 produce identical IR, the compiler is a fixed point — it reproduces itself.

## Regenerating the Bootstrap

When the self-hosted compiler source changes:

```bash
make bootstrap-update
```

This recompiles `quadc.qd` using the C++ compiler and updates `quadc.ll`.

## Architecture

The `.ll` file is architecture-independent LLVM IR. It contains no target triple, so `clang` targets the host architecture by default. Cross-compilation works with:

```bash
clang --target=aarch64-linux-gnu bootstrap/quadc.ll ...
```

## Source Files

The self-hosted compiler source lives in `lib/qdlexer/qd/lexer/`:
- `quadc.qd` — Driver (reads source, invokes parser/codegen, calls clang)
- `lexer.qd` — Tokenizer
- `ast.qd` — AST node types
- `parser.qd` — Recursive descent parser
- `codegen.qd` — Textual LLVM IR emitter
