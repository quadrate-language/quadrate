# quadlsp

Quadrate language server.

Serves the Language Server Protocol over stdin/stdout as JSON-RPC. Configure your
editor to use `quadlsp` as the language server for `.qd` files.

## Usage

```bash
quadlsp
```

## Features

| Area | Provided |
|------|----------|
| Diagnostics | Syntax and semantic errors as you type |
| Completion | Builtins, keywords, user functions and stdlib names |
| Hover | Signatures and documentation |
| Navigation | Go to definition, find references, document and workspace symbols |
| Hierarchies | Call hierarchy and type hierarchy |
| Editing | Rename, code actions, linked editing, on-type formatting |
| Display | Semantic tokens, inlay hints, folding ranges, code lens, document links |

Also implemented: signature help, selection ranges, document highlight, and range
formatting.

## Editor integration

- **Neovim**: https://github.com/quadrate-language/quadrate.nvim
- **VS Code**: https://github.com/quadrate-language/quadrate-vscode

## Tests

The protocol tests are Python and live in `tests/` beside this file; run them with
`./tests/run_all.sh --suite lsp` from the repository root.

`quadlsp --help` is the authoritative option list. Every Quadrate tool accepts
`-h`/`--help`, `-v`/`--version` and `--no-color`.
