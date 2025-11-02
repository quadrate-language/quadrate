# Tree-sitter Grammar for Quadrate

Tree-sitter parser for the Quadrate programming language.

## Features

- Full syntax parsing for Quadrate
- Accurate, incremental parsing
- Syntax highlighting via queries
- Indentation support
- Works with nvim-treesitter

## Building the Parser

### Prerequisites

```bash
npm install -g tree-sitter-cli
```

### Generate Parser

```bash
cd editors/nvim/tree-sitter-quadrate
tree-sitter generate
```

This generates:
- `src/parser.c` - The C parser
- `src/tree_sitter/parser.h` - Header file

### Test the Parser

```bash
tree-sitter test
```

### Use in Neovim

The parser is automatically used by nvim-treesitter when configured (see `../plugin/quadrate.lua`).

Alternatively, manually install:

```bash
# Create parser directory
mkdir -p ~/.local/share/nvim/site/pack/tree-sitter/start/tree-sitter-quadrate

# Copy files
cp -r . ~/.local/share/nvim/site/pack/tree-sitter/start/tree-sitter-quadrate/
```

Then in Neovim:

```lua
local parser_config = require("nvim-treesitter.parsers").get_parser_configs()
parser_config.quadrate = {
  install_info = {
    url = "~/.local/share/nvim/site/pack/tree-sitter/start/tree-sitter-quadrate",
    files = {"src/parser.c"},
  },
  filetype = "quadrate",
}
```

## Grammar Structure

The grammar defines:

- **Function definitions**: `fn name( x:float -- y:float ) { ... }`
- **Stack signatures**: Parameter and return types
- **Constants**: `const name = value`
- **Use statements**: `use module`
- **Control flow**: `if`/`else`, `for`, `switch`/`case`/`default`, `break`/`continue`, `defer`
- **Built-in operations**: Stack ops, arithmetic, math, comparison, logic, I/O
- **Literals**: Numbers, strings
- **Comments**: `//` and `/* */`

## Query Files

- `queries/highlights.scm` - Syntax highlighting
- `queries/indents.scm` - Indentation rules

## Testing

The grammar has comprehensive tests covering all language features.

### Run Tests Directly

```bash
# From this directory
tree-sitter test

# Or use the test script
bash test_parser.sh
```

### Run Tests via Meson

```bash
# From quadrate repo root
meson test -C build/debug tree-sitter-grammar

# Or with all tests
make tests
```

### Test Coverage

The test suite includes:
- **functions.txt**: Function definitions with/without parameters
- **control_flow.txt**: If/else, for loops, switch, break/continue, defer
- **literals.txt**: Numbers, strings, loop variables
- **declarations.txt**: Constants, use statements
- **comments.txt**: Line and block comments
- **builtins.txt**: Stack, arithmetic, math, comparison, logic, I/O operations

Total: 20+ test cases covering all language constructs.

## Development

### Modify Grammar

1. Edit `grammar.js`
2. Run `tree-sitter generate`
3. Add/update tests in `test/corpus/`
4. Test with `tree-sitter test`
5. Update queries if needed

### Debug Parsing

```bash
tree-sitter parse examples/test.qd
tree-sitter highlight examples/test.qd
```

## See Also

- [Tree-sitter Documentation](https://tree-sitter.github.io/tree-sitter/)
- [nvim-treesitter](https://github.com/nvim-treesitter/nvim-treesitter)
- [Quadrate Language](../../../README.md)
