# llvmgen

LLVM code generator for Quadrate.

## Overview

Generates LLVM IR from the Quadrate AST. Used for JIT compilation in embedded mode and produces optimized native code.

## Key components

- **generator** - AST to LLVM IR translation
- Runtime function declarations
- Type mapping (Quadrate types → LLVM types)

## Dependencies

Requires LLVM 14+.
