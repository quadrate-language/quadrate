#!/bin/bash
# Start the MkDocs development server with Quadrate syntax highlighting

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Create venv if it doesn't exist
if [ ! -d ".venv" ]; then
    echo "Creating virtual environment..."
    python -m venv .venv
fi

# Activate venv
source .venv/bin/activate

# Install dependencies if needed
if ! python -c "import mkdocs" 2>/dev/null; then
    echo "Installing dependencies..."
    pip install -q mkdocs pygments
fi

# Install Quadrate lexer if needed
if ! python -c "from quadrate_lexer import QuadrateLexer" 2>/dev/null; then
    echo "Installing Quadrate syntax highlighter..."
    pip install -q -e pygments-quadrate
fi

echo "Starting MkDocs server..."
echo "Open http://127.0.0.1:8000 in your browser"
echo ""

mkdocs serve
