# quadmcp

MCP (Model Context Protocol) server for Quadrate language documentation.

Provides tools to query standard library documentation, builtins, and language syntax.

## Build

```bash
quad build quadmcp.qd -o quadmcp
```

For optimized build:
```bash
quad build -O3 quadmcp.qd -o quadmcp
```

## Usage

### Stdio mode (default, for MCP clients)

```bash
./quadmcp
```

### HTTP mode

```bash
./quadmcp --http
```

Server starts on http://localhost:3000

## HTTP Endpoints

- `GET /` - Server info
- `POST /mcp` - JSON-RPC endpoint
- `GET /mcp/sse` - SSE endpoint

## MCP Methods

- `initialize` - Initialize MCP session
- `tools/list` - List available tools
- `tools/call` - Call a tool
- `resources/list` - List resources (empty)
- `prompts/list` - List prompts (empty)
- `ping` - Health check

## Tools

### quadrate_list_modules

List all available standard library modules with function/constant counts.

### quadrate_get_module

Get full documentation for a specific module.

**Arguments:**
- `name` (string): Module name (e.g., "math", "str", "io")

### quadrate_get_function

Get documentation for a specific function in a module.

**Arguments:**
- `module` (string): Module name
- `function` (string): Function name

### quadrate_search

Search documentation for a query string.

**Arguments:**
- `query` (string): Search query (case-insensitive)

### quadrate_get_builtins

Get all builtin instructions organized by category.

### quadrate_get_builtin

Get documentation for a specific builtin instruction.

**Arguments:**
- `name` (string): Instruction name or alias (e.g., "dup", "swap", "+")

### quadrate_get_syntax

Get language syntax reference (types, keywords, operators).

### quadrate_get_keyword

Get documentation for a specific keyword.

**Arguments:**
- `name` (string): Keyword name (e.g., "fn", "if", "struct")

### quadrate_get_operator

Get documentation for a specific operator.

**Arguments:**
- `name` (string): Operator symbol or alias (e.g., "+", "==", "->")

## Examples

### Stdio mode

```bash
# Initialize
echo '{"jsonrpc":"2.0","method":"initialize","id":1}' | ./quadmcp

# List tools
echo '{"jsonrpc":"2.0","method":"tools/list","id":2}' | ./quadmcp

# List modules
echo '{"jsonrpc":"2.0","method":"tools/call","id":3,"params":{"name":"quadrate_list_modules","arguments":{}}}' | ./quadmcp

# Get module docs
echo '{"jsonrpc":"2.0","method":"tools/call","id":4,"params":{"name":"quadrate_get_module","arguments":{"name":"str"}}}' | ./quadmcp
```

### HTTP mode

```bash
# Start server
./quadmcp --http

# Initialize
curl -X POST http://localhost:3000/mcp \
  -d '{"jsonrpc":"2.0","method":"initialize","id":1}'

# List modules
curl -X POST http://localhost:3000/mcp \
  -d '{"jsonrpc":"2.0","method":"tools/call","id":2,"params":{"name":"quadrate_list_modules","arguments":{}}}'
```
