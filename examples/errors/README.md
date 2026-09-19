# Error handling

Demonstrates fallible functions and error handling.

## Run

```bash
quad run errors.qd
```

## Features

- Fallible functions (`fn name()!`) and raising with `panic`
- Error checking with `if`/`else`
- Reading the error channel directly with `err` (a message and a code, and reading empties it)
- Reading it as one value with `error::last`, which can be kept: the example collects several
  failures into an array while the loop carries on
- Adding context with `error::wrap`, which keeps the original code so a caller matching on it
  still matches, and `error::root` to walk a chain back to what actually failed
