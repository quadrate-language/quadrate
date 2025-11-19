#!/bin/bash
# Test script to verify Neovim plugin installation

set -e

echo "=== Quadrate Neovim Plugin Installation Test ==="
echo

# Check if nvim is installed
if ! command -v nvim &> /dev/null; then
    echo "❌ Neovim not found. Please install Neovim 0.8+"
    exit 1
fi
echo "✓ Neovim found: $(nvim --version | head -n1)"

# Check Neovim version
NVIM_VERSION=$(nvim --version | head -n1 | grep -oP 'v\K[0-9]+\.[0-9]+' | head -n1)
if [ $(echo "$NVIM_VERSION < 0.8" | bc -l 2>/dev/null || echo "1") -eq 1 ] && [ "$NVIM_VERSION" != "0.8" ] && [ "$NVIM_VERSION" != "0.9" ] && [ "$NVIM_VERSION" != "0.10" ]; then
    echo "⚠️  Neovim version may be too old (need 0.8+)"
fi

# Check if quadlsp is available
if command -v quadlsp &> /dev/null; then
    echo "✓ quadlsp found in PATH"
elif [ -f "../../build/debug/cmd/quadlsp/quadlsp" ]; then
    echo "✓ quadlsp found in build directory"
    echo "  (Consider running 'make install' or adding to PATH)"
else
    echo "❌ quadlsp not found"
    echo "  Run 'make install' from quadrate root"
    exit 1
fi

# Check if nvim-lspconfig is installed
LSPCONFIG_CHECK=$(nvim --headless -c "lua if pcall(require, 'lspconfig') then print('OK') else print('NOT_FOUND') end" -c "quit" 2>&1 | grep -o "OK\|NOT_FOUND" || echo "NOT_FOUND")

if [ "$LSPCONFIG_CHECK" = "OK" ]; then
    echo "✓ nvim-lspconfig is installed"
else
    echo "❌ nvim-lspconfig not found"
    echo "  Install it via your plugin manager:"
    echo "    Lazy: { 'neovim/nvim-lspconfig' }"
    echo "    Packer: use 'neovim/nvim-lspconfig'"
    exit 1
fi

# Check if files exist
echo
echo "Checking plugin files:"
if [ -f "ftdetect/quadrate.vim" ]; then
    echo "✓ ftdetect/quadrate.vim exists"
else
    echo "❌ ftdetect/quadrate.vim not found"
fi

if [ -f "plugin/quadrate.lua" ]; then
    echo "✓ plugin/quadrate.lua exists"
else
    echo "❌ plugin/quadrate.lua not found"
fi

# Suggest installation
echo
echo "=== Installation Instructions ==="
echo
echo "Copy files to your Neovim config:"
echo "  cp -r $(pwd)/* ~/.config/nvim/"
echo
echo "Or symlink for development:"
echo "  ln -s $(pwd)/ftdetect/quadrate.vim ~/.config/nvim/ftdetect/"
echo "  ln -s $(pwd)/plugin/quadrate.lua ~/.config/nvim/plugin/"
echo
echo "Then open a .qd file in Neovim to test!"
echo
echo "Test with: nvim /tmp/test.qd"
