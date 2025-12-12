# qdsignal

Unix signal handling (`signal::` module).

## Overview

Polling-based signal handling API. Signals are caught and stored as flags
which can be checked and cleared at safe points in the program.

## Key Functions

- `trap` - Install handler to catch a signal
- `ignore` - Ignore a signal completely
- `reset` - Restore default signal behavior
- `pending` - Check if a signal was received
- `clear` - Clear the pending flag
- `wait` - Block until any signal is received

## Constants

Common signals: `SIGINT`, `SIGTERM`, `SIGHUP`, `SIGQUIT`, `SIGPIPE`,
`SIGUSR1`, `SIGUSR2`, `SIGALRM`, `SIGCHLD`
