#!/bin/bash
# Test script for tree-sitter grammar

set -e

cd "$(dirname "$0")"

echo "=== Tree-sitter Quadrate Grammar Tests ==="
echo

# Check if tree-sitter is installed
if ! command -v tree-sitter &> /dev/null; then
    echo "❌ tree-sitter CLI not found"
    echo "Install with: npm install -g tree-sitter-cli"
    exit 1
fi
echo "✓ tree-sitter found: $(tree-sitter --version)"

# Check if parser is generated
if [ ! -f "src/parser.c" ]; then
    echo "⚠️  Parser not generated, running 'tree-sitter generate'..."
    tree-sitter generate
fi
echo "✓ Parser generated"

# Run tests
echo
echo "=== Running tree-sitter tests ==="
tree-sitter test

echo
echo "✓ All tree-sitter tests passed!"
