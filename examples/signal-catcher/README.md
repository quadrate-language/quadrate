# Signal catcher

Demonstrates graceful signal handling with Ctrl+C.

## Run

```bash
quadc -r signal-catcher.qd
```

Then press Ctrl+C to trigger a graceful shutdown.

## Features

- `signal::trap` to catch SIGINT
- `signal::pending` to poll for signals
- Graceful shutdown pattern
