# quadmcp

Quadrate MCP server.

Serves the Model Context Protocol over stdin/stdout as JSON-RPC, giving an MCP
client the Quadrate language and standard library reference. Written in Quadrate
itself, so the Makefile builds it after the compiler.

## Usage

```bash
quadmcp            # Serve MCP on stdin/stdout (how a client starts it)
quadmcp --http     # Serve MCP over HTTP on localhost:3000
```

Configure your MCP client to run `quadmcp` as a stdio-based server.

## Build

```bash
make quadmcp
```

## Options

| Option | Description |
|--------|-------------|
| `--http` | Serve over HTTP on 127.0.0.1:3000 instead of stdio |
| `--host <ip>` | Bind `--http` to this address instead of 127.0.0.1 |

## Tools

| Tool | Description |
|------|-------------|
| `quadrate_list_modules` | List standard library modules |
| `quadrate_get_module` | Get module documentation |
| `quadrate_get_function` | Get function documentation |
| `quadrate_search` | Search all documentation |
| `quadrate_get_builtins` | Get builtin instructions |
| `quadrate_get_builtin` | Get specific builtin |
| `quadrate_get_syntax` | Get language syntax reference |
| `quadrate_get_keyword` | Get keyword documentation |
| `quadrate_get_operator` | Get operator documentation |
| `quadrate_find_function` | Fuzzy search for functions |
| `quadrate_search_by_signature` | Find functions by signature |
| `quadrate_get_error` | Get error constant documentation |
| `quadrate_trace_stack` | Stack operation reference |
| `quadrate_generate_template` | Generate code templates |
| `quadrate_explain_signature` | Explain function signatures |
| `quadrate_type_conversion` | Type conversion guide |

## Resources

- `quadrate://snippets/*` - Code examples
- `quadrate://guides/*` - Best practices and common mistakes

## Source Files

Files are concatenated during build:

- `core.qd` - Imports, constants, helpers
- `tools.qd` - Tool implementations
- `resources.qd` - Resource content
- `server.qd` - Dispatch and main

`quadmcp --help` is the authoritative option list. Every Quadrate tool accepts
`-h`/`--help`, `-v`/`--version` and `--no-color`.
