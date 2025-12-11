# Embedding API

Demonstrates embedding Quadrate in C/C++ applications.

## Build

```bash
meson compile -C build/debug examples/embed/embed
```

## Files

- `main.cc` - Basic embedding
- `multi-module-test.cc` - Multiple modules
- `native-functions-test.cc` - Registering C functions
- `incremental-test.cc` - Incremental compilation

## Features

- Module creation
- Script compilation
- Native function registration
- Code execution
