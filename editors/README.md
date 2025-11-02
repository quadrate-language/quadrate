# Editor Integrations for Quadrate

This directory contains editor configurations and plugins for the Quadrate programming language.

## Available Integrations

### Neovim (`nvim/`)

**Status**: ✅ Complete

Full Language Server Protocol integration for Neovim with:
- **Syntax Highlighting**: Tree-sitter parser (modern, accurate) + Vim regex fallback
- **Auto-completion**: 47 built-in instructions
- **Real-time diagnostics**: Syntax error detection
- **Document formatting**: LSP formatting support
- **Smart indentation**: Context-aware indentation
- **Filetype detection**: Automatic `.qd` recognition

See [nvim/README.md](nvim/README.md) for installation and usage.

### VSCode / VSCodium

**Status**: 📝 Planned

To use with VSCode, install a generic LSP client extension and configure:

```json
{
  "lsp.servers": {
    "quadrate": {
      "command": "quadlsp",
      "filetypes": ["quadrate"],
      "rootPatterns": [".git/"]
    }
  }
}
```

Example extensions:
- [vscode-languageclient](https://marketplace.visualstudio.com/items?itemName=vscode.vscode-languageclient)

### Vim (classic)

**Status**: 📝 Planned

Use with vim-lsp:

```vim
if executable('quadlsp')
    au User lsp_setup call lsp#register_server({
        \ 'name': 'quadlsp',
        \ 'cmd': {server_info->['quadlsp']},
        \ 'allowlist': ['quadrate'],
        \ })
endif

au BufRead,BufNewFile *.qd set filetype=quadrate
```

### Emacs

**Status**: 📝 Planned

Use with lsp-mode:

```elisp
(add-to-list 'auto-mode-alist '("\\.qd\\'" . quadrate-mode))

(with-eval-after-load 'lsp-mode
  (add-to-list 'lsp-language-id-configuration '(quadrate-mode . "quadrate"))

  (lsp-register-client
   (make-lsp-client :new-connection (lsp-stdio-connection "quadlsp")
                    :major-modes '(quadrate-mode)
                    :server-id 'quadlsp)))
```

### Helix

**Status**: 📝 Planned

Add to `~/.config/helix/languages.toml`:

```toml
[[language]]
name = "quadrate"
file-types = ["qd"]
language-server = { command = "quadlsp" }
```

### Kate / KWrite

**Status**: 📝 Planned

Add LSP client configuration to `~/.config/kate/lspclient/settings.json`:

```json
{
  "servers": {
    "quadrate": {
      "command": ["quadlsp"],
      "root": "",
      "url": "https://github.com/yourusername/quadrate"
    }
  }
}
```

## LSP Server

All integrations use the `quadlsp` Language Server Protocol server.

### Installation

```bash
# From quadrate repo root
make install
```

This installs `quadlsp` to `/usr/bin/quadlsp`.

### Features

- **Completion**: 47 built-in instructions (add, sub, mul, dup, etc.)
- **Diagnostics**: Real-time syntax error detection
- **Formatting**: Document formatting support (stub)
- **Protocol**: Full LSP compliance with extensive test coverage

See [../bin/quadlsp/README.md](../bin/quadlsp/README.md) for more details.

## Contributing

Want to add support for your favorite editor?

1. Create a directory: `editors/[editor-name]/`
2. Add configuration files
3. Write a README with installation instructions
4. Test thoroughly
5. Submit patches to the mailing list: ~klahr/quadrate@lists.sr.ht

### Editor Plugin Guidelines

When creating editor integrations:

- Use the standard `quadlsp` binary (don't fork)
- Follow the editor's plugin conventions
- Provide clear installation instructions
- Include troubleshooting section
- Test with various Quadrate code samples
- Document key mappings/commands

## Quick Start

1. **Install quadlsp**: `make install`
2. **Choose your editor**: Navigate to the editor's directory
3. **Follow the README**: Each editor has specific setup instructions
4. **Test**: Open a `.qd` file and verify LSP features work

## Testing Your Integration

Create a test file:

```quadrate
fn fibonacci( n:float -- result:float ) {
    n 0 eq if {
        0
    } else {
        n 1 eq if {
            1
        } else {
            n 1 sub fibonacci
            n 2 sub fibonacci
            add
        }
    }
}

fn main( -- ) {
    10 fibonacci print
}
```

Expected LSP features:
- ✅ Completion shows `add`, `sub`, `eq`, `if`, etc.
- ✅ Diagnostics detect syntax errors
- ✅ Formatting available (even if stub)
- ✅ No crashes or hangs

## License

All editor integrations are licensed under the same terms as the Quadrate project.
