# Quadrate Playground

A web-based REPL for trying Quadrate code in the browser. All code runs in a Docker sandbox.

## Setup

```bash
# Build Quadrate
make release

# Build sandbox Docker image
tools/playground/build-sandbox.sh

# Build and run playground
cd tools/playground
go build -o playground .
./playground
```

Open http://localhost:8080

## Options

| Flag | Default | Description |
|------|---------|-------------|
| `-addr` | `:8080` | Listen address |
| `-image` | `quadrate-sandbox` | Docker sandbox image |
| `-timeout` | `5s` | Execution timeout |

## Security

All code runs in an isolated Docker container with:

- No network access
- Read-only filesystem
- 64MB memory limit
- 0.5 CPU limit
- 32 process limit
- No Linux capabilities
- Unprivileged user

## Features

- Syntax highlighting
- Line numbers
- Error line highlighting
- Example programs
- Ctrl+Enter to run
