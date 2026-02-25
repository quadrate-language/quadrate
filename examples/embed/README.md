# Embedding Example

Demonstrates embedding Quadrate in a C/C++ application.

## Build

```bash
make examples
```

## Files

- `main.cc` - Basic embedding: module creation, script compilation, execution, stdlib usage

## Tests

Embedding API tests are in `tests/embed/`. See `tests/embed/` for test coverage of native function registration, typed signatures, userdata, and stdlib interop.
