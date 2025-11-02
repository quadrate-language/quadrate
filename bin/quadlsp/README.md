# Quadrate Language Server Protocol (LSP)

A simple Language Server Protocol implementation for the Quadrate programming language.

## Features

- **Diagnostics**: Basic syntax error detection (via Quadrate parser)
- **Completion**: Auto-completion for built-in instructions
- **Formatting**: Document formatting support (returns empty for now - can be integrated with quadfmt)
- **Text Synchronization**: Full document synchronization

## Building

The LSP server is built as part of the Quadrate project:

```bash
make debug    # or make release
```

The executable will be at `build/debug/bin/quadlsp/quadlsp` (or `build/release/bin/quadlsp/quadlsp`).

## Installation

To install system-wide:

```bash
make install
```

This will install `quadlsp` to `/usr/bin/` (or `$PREFIX/bin/` if PREFIX is set).

## Usage

The LSP server communicates via stdin/stdout using the Language Server Protocol specification.

### VSCode/VSCodium

Create a VSCode extension or use an existing generic LSP client extension and configure it to use `quadlsp`:

```json
{
  "command": "/path/to/quadlsp",
  "filetypes": ["quadrate"],
  "rootPatterns": [".git"]
}
```

### Neovim

Using `nvim-lspconfig`:

```lua
local lspconfig = require('lspconfig')
local configs = require('lspconfig.configs')

-- Define quadlsp if not already defined
if not configs.quadlsp then
  configs.quadlsp = {
    default_config = {
      cmd = {'/path/to/quadlsp'},
      filetypes = {'quadrate'},
      root_dir = lspconfig.util.root_pattern('.git'),
      settings = {},
    },
  }
end

-- Setup quadlsp
lspconfig.quadlsp.setup{}
```

### Vim/Neovim with vim-lsp

```vim
if executable('quadlsp')
    au User lsp_setup call lsp#register_server({
        \ 'name': 'quadlsp',
        \ 'cmd': {server_info->['quadlsp']},
        \ 'allowlist': ['quadrate'],
        \ })
endif
```

### Emacs with lsp-mode

```elisp
(add-to-list 'lsp-language-id-configuration '(quadrate-mode . "quadrate"))

(lsp-register-client
 (make-lsp-client :new-connection (lsp-stdio-connection "/path/to/quadlsp")
                  :major-modes '(quadrate-mode)
                  :server-id 'quadlsp))
```

## Protocol Support

Currently implemented LSP methods:

- `initialize` - Server initialization
- `initialized` - Initialization notification
- `shutdown` - Server shutdown
- `exit` - Server exit
- `textDocument/didOpen` - Document opened
- `textDocument/didChange` - Document changed (full sync)
- `textDocument/didSave` - Document saved
- `textDocument/completion` - Code completion
- `textDocument/formatting` - Document formatting (stub)

## Capabilities

- **Text Document Sync**: Full (mode 1)
- **Completion Provider**: Basic built-in instruction completion
- **Formatting Provider**: Enabled (integration with quadfmt TODO)

## Current Limitations

1. **Diagnostics**: Currently shows a generic error message when parsing fails. Detailed error messages are printed to stderr but not yet captured and sent as individual diagnostics.
2. **Formatting**: Returns empty edits (needs integration with quadfmt tool)
3. **No hover/go-to-definition**: Not yet implemented
4. **No incremental sync**: Only supports full document synchronization

## Future Improvements

- [ ] Integrate custom ErrorReporter with Ast class for detailed diagnostics
- [ ] Implement document formatting using quadfmt
- [ ] Add hover information for built-in instructions
- [ ] Implement go-to-definition for functions
- [ ] Add incremental text synchronization
- [ ] Implement semantic tokens for better syntax highlighting
- [ ] Add signature help for functions

## Testing

The LSP server includes a comprehensive test suite that verifies all major functionality.

### Running Tests

```bash
# Run all tests (including LSP tests)
make tests

# Or run LSP tests specifically via meson
meson test -C build/debug test_lsp --print-errorlogs

# Or run the Python test script directly
python3 bin/quadlsp/tests/test_lsp.py
```

### Test Coverage

The test suite (`bin/quadlsp/tests/test_lsp.py`) includes:

- **Initialize/Shutdown**: Tests server initialization and graceful shutdown
- **Completion**: Tests auto-completion for built-in instructions (47 items)
- **Diagnostics**: Tests that invalid code doesn't crash the server
- **JSON-RPC**: Validates proper JSON-RPC 2.0 protocol compliance

All tests run automatically as part of the meson test suite.

### Manual Testing

You can also test the LSP manually:

```bash
# Test initialize and shutdown
echo -n -e 'Content-Length: 113\r\n\r\n{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{},"rootUri":"file:///tmp"}}' | ./quadlsp
```

## License

Same as the Quadrate project.
