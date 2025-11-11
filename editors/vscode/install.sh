#!/bin/bash
# Quick installation script for Quadrate VSCode extension

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "==================================="
echo "  Quadrate VSCode Extension Setup"
echo "==================================="
echo ""

# Check for npm
if ! command -v npm &> /dev/null; then
    echo "Error: npm is not installed"
    echo "Please install Node.js and npm first"
    exit 1
fi

# Check for quadlsp
if ! command -v quadlsp &> /dev/null; then
    echo "Warning: quadlsp not found in PATH"
    echo "Please run 'make install' from the quadrate repo root"
    echo ""
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Install dependencies
echo "Installing npm dependencies..."
npm install

# Compile TypeScript
echo "Compiling TypeScript..."
npm run compile

# Detect VSCode/VSCodium
VSCODE_EXT_DIR=""
if [ -d "$HOME/.vscode/extensions" ]; then
    VSCODE_EXT_DIR="$HOME/.vscode/extensions"
    EDITOR_NAME="VSCode"
elif [ -d "$HOME/.vscode-oss/extensions" ]; then
    VSCODE_EXT_DIR="$HOME/.vscode-oss/extensions"
    EDITOR_NAME="VSCodium"
else
    echo ""
    echo "Error: Could not find VSCode or VSCodium extensions directory"
    echo "Expected one of:"
    echo "  ~/.vscode/extensions"
    echo "  ~/.vscode-oss/extensions"
    echo ""
    echo "Is VSCode/VSCodium installed?"
    exit 1
fi

# Install extension via symlink
EXT_NAME="quadrate-0.1.0"
TARGET_DIR="$VSCODE_EXT_DIR/$EXT_NAME"

if [ -L "$TARGET_DIR" ]; then
    echo ""
    echo "Extension already installed at: $TARGET_DIR"
    read -p "Reinstall? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm "$TARGET_DIR"
    else
        echo "Keeping existing installation"
        exit 0
    fi
fi

echo ""
echo "Installing extension to $EDITOR_NAME..."
ln -s "$SCRIPT_DIR" "$TARGET_DIR"

echo ""
echo "==================================="
echo "  Installation Complete!"
echo "==================================="
echo ""
echo "Next steps:"
echo "1. Reload $EDITOR_NAME (Ctrl+Shift+P → 'Developer: Reload Window')"
echo "2. Open a .qd file or the test-example.qd file"
echo "3. Verify syntax highlighting and LSP features work"
echo ""
echo "For troubleshooting, see: $SCRIPT_DIR/README.md"
