# Editor Integrations

Editor plugins and configurations for Quadrate.

## Available

- **nvim/** - Neovim with tree-sitter and LSP
- **vscode/** - VS Code extension

## LSP Server

All editors use `quadlsp` for language features:

```bash
make install  # Installs quadlsp
```

## Adding Support

1. Create `editors/<editor>/`
2. Add configuration files
3. Write README with setup instructions
