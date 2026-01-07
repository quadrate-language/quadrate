# qdrt

Runtime library for Quadrate programs.

## Overview

Provides the core stack machine implementation, execution context, and built-in operations. All compiled Quadrate programs link against this library.

## Key components

- **context** - Execution context with stack and error state
- **stack** - Value stack with type-tagged slots
- **runtime** - Built-in operations (arithmetic, stack manipulation, I/O)
- **array** - Dynamic array implementation
- **qd_string** - Reference-counted string with builder

## Platform support

Threading primitives are abstracted in `src/platform/` for portability. Currently supports POSIX systems.
