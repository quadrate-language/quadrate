# Quadrate LSP for Neovim

Neovim configuration for the Quadrate programming language with Language Server Protocol support.

## Features

- **Syntax Highlighting**:
  - Tree-sitter parser (modern, accurate, fast) - **recommended**
  - Vim regex syntax (fallback for compatibility)
- **LSP Integration**: Full LSP support via `nvim-lspconfig`
- **Auto-completion**: 47 built-in instructions (add, sub, mul, dup, swap, etc.)
- **Diagnostics**: Real-time syntax error detection
- **Formatting**: Document formatting support (via `<space>f`)
- **Smart Indentation**: Automatic indentation based on braces
- **Comment Support**: Line (`//`) and block (`/* */`) comments
- **Filetype Detection**: Automatic `.qd` file recognition

## Prerequisites

### Required
1. **Neovim 0.8+** (for LSP support)
2. **nvim-lspconfig** plugin
3. **quadlsp** binary installed

### Optional (for Tree-sitter)
4. **nvim-treesitter** plugin (for better syntax highlighting)
5. **tree-sitter-cli** (to build the parser)
6. **Node.js** (for tree-sitter generation)

Install quadlsp:
```bash
cd /path/to/quadrate
make install
# This installs quadlsp to /usr/bin/quadlsp
```

Or use local build:
```bash
make debug
# Binary at: build/debug/cmd/quadlsp/quadlsp
```

## Installation

### Method 1: Copy to Neovim Config (Recommended)

Copy the plugin files to your Neovim config directory:

```bash
# From quadrate repo root
cp -r editors/nvim/* ~/.config/nvim/

# Or for specific locations:
cp editors/nvim/ftdetect/quadrate.vim ~/.config/nvim/ftdetect/
cp editors/nvim/plugin/quadrate.lua ~/.config/nvim/plugin/
```

### Method 2: Symlink (For Development)

Symlink for automatic updates:

```bash
ln -s /path/to/quadrate/editors/nvim/ftdetect/quadrate.vim ~/.config/nvim/ftdetect/
ln -s /path/to/quadrate/editors/nvim/plugin/quadrate.lua ~/.config/nvim/plugin/
```

### Method 3: Plugin Manager

#### lazy.nvim

```lua
{
  dir = '/path/to/quadrate/editors/nvim',
  dependencies = { 'neovim/nvim-lspconfig' },
}
```

#### packer.nvim

```lua
use {
  '/path/to/quadrate/editors/nvim',
  requires = { 'neovim/nvim-lspconfig' },
}
```

#### vim-plug

```vim
Plug '/path/to/quadrate/editors/nvim'
```

## Configuration

### Tree-sitter Setup (Optional but Recommended)

Tree-sitter provides faster and more accurate syntax highlighting than traditional Vim syntax files.

#### Prerequisites

Install tree-sitter CLI and nvim-treesitter:

```bash
npm install -g tree-sitter-cli
```

Install nvim-treesitter plugin (using your plugin manager):

```lua
-- lazy.nvim
{ 'nvim-treesitter/nvim-treesitter', build = ':TSUpdate' }

-- packer.nvim
use { 'nvim-treesitter/nvim-treesitter', run = ':TSUpdate' }
```

#### Build the Quadrate Parser

```bash
# From quadrate repo root
cd editors/nvim/tree-sitter-quadrate

# Generate the parser
tree-sitter generate

# Test it (optional)
tree-sitter test
```

#### Copy Parser to Neovim Config

```bash
# Copy the tree-sitter parser to your Neovim config
cp -r editors/nvim/tree-sitter-quadrate ~/.config/nvim/

# Or symlink for development
ln -s $(pwd)/editors/nvim/tree-sitter-quadrate ~/.config/nvim/tree-sitter-quadrate
```

#### Verify Tree-sitter Works

Open a `.qd` file in Neovim and check:

```vim
:TSInstallInfo quadrate
" Should show 'quadrate' parser

:TSBufEnable highlight
" Enables tree-sitter highlighting
```

**Note**: If tree-sitter is not installed, the plugin will automatically fall back to traditional Vim syntax highlighting.

### Using Local Build

If you haven't installed quadlsp system-wide, modify the cmd in `plugin/quadrate.lua`:

```lua
configs.quadlsp = {
  default_config = {
    cmd = {'/path/to/quadrate/build/debug/cmd/quadlsp/quadlsp'},
    -- ... rest of config
  }
}
```

Or set an alias in your shell:
```bash
# In ~/.bashrc or ~/.zshrc
alias quadlsp='/path/to/quadrate/build/debug/cmd/quadlsp/quadlsp'
```

### Custom Key Mappings

The default key mappings are:

| Key | Action |
|-----|--------|
| `K` | Hover documentation |
| `gd` | Go to definition |
| `gD` | Go to declaration |
| `gi` | Go to implementation |
| `gr` | Find references |
| `<space>f` | Format document |
| `<space>rn` | Rename symbol |
| `<space>ca` | Code actions |
| `<space>e` | Show diagnostics |
| `[d` | Previous diagnostic |
| `]d` | Next diagnostic |

To customize, edit `editors/nvim/plugin/quadrate.lua` in the `on_attach` function.

## Usage

1. Open a `.qd` file in Neovim
2. The LSP will automatically attach
3. You'll see "Quadrate LSP attached" notification
4. Start typing to see completions (use `<C-x><C-o>` for manual completion)

### Example Session

```vim
:e test.qd
" Type 'fn main( -- ) {' then trigger completion
" Type 'ad' and press <C-x><C-o> to see 'add' completion
```

## Completion Items

The LSP provides auto-completion for 47 built-in instructions:

**Arithmetic**: add, sub, mul, div, inc, dec, abs, sqrt, sq, pow, mod

**Stack Operations**: dup, swap, drop, over, rot, nip, tuck, pick, roll, dup2, swap2, over2, drop2

**Comparison**: eq, neq, lt, gt, lte, gte, within

**Logic**: and, or, not

**Math Functions**: sin, cos, tan, asin, acos, atan, ln, log10, ceil, floor, round, min, max

**Control Flow**: if, for, switch, case, default, break, continue, defer

**I/O**: print, prints, printv, printsv

**Stack Introspection**: depth, clear

## Troubleshooting

### LSP Not Starting

Check if quadlsp is in your PATH:
```bash
which quadlsp
# or
quadlsp --version  # (if implemented)
```

Enable LSP logs in Neovim:
```lua
vim.lsp.set_log_level('debug')
:lua vim.cmd('e ' .. vim.lsp.get_log_path())
```

### No Completions

1. Verify LSP is attached: `:LspInfo`
2. Check if omnifunc is set: `:set omnifunc?`
3. Try manual completion: `<C-x><C-o>`

### Syntax Errors Not Showing

Diagnostics are published on file open/save. Try:
1. Save the file: `:w`
2. Close and reopen: `:e`
3. Check diagnostics: `:lua vim.diagnostic.open_float()`

## Advanced Configuration

### With nvim-cmp (Auto-completion)

If you use `nvim-cmp`, it will automatically use LSP completions. No extra config needed.

```lua
require('cmp').setup({
  sources = {
    { name = 'nvim_lsp' },
    -- ... other sources
  }
})
```

### Custom Capabilities

To add custom capabilities (e.g., for snippets):

```lua
local capabilities = require('cmp_nvim_lsp').default_capabilities()

lspconfig.quadlsp.setup({
  capabilities = capabilities,
  -- ... rest of config
})
```

### Disable Auto-attach

To prevent auto-attach and setup manually:

Comment out the autocmd in `plugin/quadrate.lua` and call:
```lua
:lua require('lspconfig').quadlsp.setup({})
```

## Testing the LSP

Test if the LSP is working:

```bash
# Create a test file
cat > test.qd << 'EOF'
fn main( -- ) {
    5 10 add
    print
}
EOF

# Open in Neovim
nvim test.qd
```

You should see:
- "Quadrate LSP attached" notification
- Syntax highlighting for keywords
- Hover over `add` with `K` should show information (when implemented)
- `<C-x><C-o>` shows completion menu

## Testing the Tree-sitter Grammar

The tree-sitter grammar has comprehensive automated tests:

```bash
# From quadrate repo root
cd editors/nvim/tree-sitter-quadrate

# Run tree-sitter tests
tree-sitter test

# Or use test script
bash test_parser.sh
```

Test coverage:
- 20+ test cases covering all language constructs
- Functions, control flow, literals, comments, built-ins
- Automatically run with `make tests` from repo root

See [tree-sitter-quadrate/README.md](tree-sitter-quadrate/README.md) for details.

## See Also

- [Tree-sitter Grammar Documentation](tree-sitter-quadrate/README.md)
- [Quadrate LSP Documentation](../../cmd/quadlsp/README.md)
- [nvim-lspconfig Documentation](https://github.com/neovim/nvim-lspconfig)
- [Neovim LSP Guide](https://neovim.io/doc/user/lsp.html)
- [Tree-sitter Guide](https://tree-sitter.github.io/tree-sitter/)

## Contributing

To improve this plugin:

1. Edit files in `editors/nvim/`
2. Test with a `.qd` file
3. Submit patches to the Quadrate mailing list

## License

Same as the Quadrate project.
