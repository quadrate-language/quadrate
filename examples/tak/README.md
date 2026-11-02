# Tak (Takeuchi Function)

The Takeuchi function, a classic recursive benchmark.

## Run

```bash
quad run tak.qd
```

## Features

- Triple recursion
- Stack code with three inputs, each used three times, factored into one-line words:
  `step` makes one of the inner calls and turns the triple to the next one's order
- `inline fn`: each call to a helper is replaced by its body, so the factored version runs as
  fast as one written out in a piece
