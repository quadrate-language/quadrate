#!/bin/bash
# Usage: benchmarks/run_one.sh <name> <title> [CFLAGS_EXTRA] [NODE_FLAGS]
# Compiles and runs benchmarks/<name>.{qd,c,rs,go,js,cs,py}. Run from the repository root.

if [ $# -lt 2 ]; then
    echo "Usage: $0 <name> <title> [CFLAGS_EXTRA] [NODE_FLAGS]"
    exit 1
fi

NAME="$1"
TITLE="$2"
CFLAGS_EXTRA="$3"
NODE_FLAGS="$4"
B="benchmarks/$NAME"

echo "=========================================="
echo "  $TITLE"
echo "=========================================="

QUADC="dist/bin/quadc"

if [ ! -f "$QUADC" ]; then
    echo "Error: quadc not found in dist/bin. Run 'make release' first."
    exit 1
fi

echo "Using compiler: $QUADC"

# Set library path to dist/lib where .a files are
export QUADRATE_LIBDIR="dist/lib"
if [ "$(uname -s)" = "Haiku" ]; then
    export QUADRATE_ROOT="dist/data/quadrate"
else
    export QUADRATE_ROOT="dist/share/quadrate"
fi
export LD_LIBRARY_PATH="dist/lib:$LD_LIBRARY_PATH"

# Compile and Run Quadrate
echo "Compiling $B.qd..."
if $QUADC "$B.qd" -O3 -o "${B}_qd"; then
    echo ""
    "./${B}_qd"
fi

# Compile and Run C
echo ""
echo "Compiling C benchmark..."
if gcc -O3 "$B.c" -o "${B}_c" $CFLAGS_EXTRA; then
    "./${B}_c"
fi

# Compile and Run Rust
echo ""
echo "Compiling Rust benchmark..."
if rustc -O "$B.rs" -o "${B}_rust" 2>/dev/null; then
    "./${B}_rust"
fi

# Compile and Run Go
echo ""
echo "Compiling Go benchmark..."
if go build -o "${B}_go" "$B.go" 2>/dev/null; then
    "./${B}_go"
fi

# Run Node.js
echo ""
if command -v node &> /dev/null; then
    node $NODE_FLAGS "$B.js"
fi

# Run C#
echo ""
if command -v dotnet &> /dev/null; then
    echo "Compiling C# benchmark..."
    cp "$B.cs" benchmarks/csharp/Program.cs
    dotnet run --project benchmarks/csharp -c Release 2>/dev/null
fi

# Run Python
echo ""
if command -v python3 &> /dev/null; then
    python3 "$B.py"
fi

echo ""
echo "=========================================="
