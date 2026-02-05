# quadmcp

MCP server for Quadrate language documentation.

## Build

```bash
make quadmcp
```

## Usage

```bash
# Stdio mode (for MCP clients)
./quadmcp

# HTTP mode (localhost:3000)
./quadmcp --http
```

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
