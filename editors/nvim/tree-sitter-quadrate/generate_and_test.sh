#!/bin/bash
# Wrapper script to generate parser and run tree-sitter tests
# This ensures the parser is generated before running tests (needed because src/ is gitignored)

set -e

cd "$(dirname "$0")"

# Generate parser if src/parser.c doesn't exist
if [ ! -f src/parser.c ]; then
    echo "Generating tree-sitter parser..."
    tree-sitter generate --no-bindings
fi

# Run tests
exec tree-sitter test
