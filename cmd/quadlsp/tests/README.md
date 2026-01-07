# LSP tests

Test suite for the Quadrate Language Server.

## Test files

- `test_lsp.py` - Basic functionality (21 tests)
- `test_lsp_extended.py` - Edge cases (35 tests)
- `test_lsp_stress.py` - Performance/load (13 tests)

## Run

```bash
meson test -C build/debug test_lsp test_lsp_extended test_lsp_stress --print-errorlogs
```

## Coverage

- Protocol compliance
- Completion
- Diagnostics
- Document lifecycle
- Formatting
- Unicode support
