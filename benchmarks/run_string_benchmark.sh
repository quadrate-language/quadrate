#!/bin/bash

echo "=========================================="
echo "  String Performance Benchmark"
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
echo "Compiling benchmarks/string_ops.qd..."
$QUADC benchmarks/string_ops.qd -O3 -o benchmarks/string_ops
if [ $? -eq 0 ]; then
    echo ""
    ./benchmarks/string_ops
fi

# Compile and Run C
echo ""
echo "Compiling C benchmark..."
gcc -O3 benchmarks/string_ops.c -o benchmarks/string_ops_c
if [ $? -eq 0 ]; then
    ./benchmarks/string_ops_c
fi

# Compile and Run Rust
echo ""
echo "Compiling Rust benchmark..."
rustc -O benchmarks/string_ops.rs -o benchmarks/string_ops_rust 2>/dev/null
if [ $? -eq 0 ]; then
    ./benchmarks/string_ops_rust
fi

# Compile and Run Go
echo ""
echo "Compiling Go benchmark..."
go build -o benchmarks/string_ops_go benchmarks/string_ops.go 2>/dev/null
if [ $? -eq 0 ]; then
    ./benchmarks/string_ops_go
fi

# Run Node.js
echo ""
if command -v node &> /dev/null; then
    node benchmarks/string_ops.js
fi

# Run C#
echo ""
if command -v dotnet &> /dev/null; then
    cp benchmarks/string_ops.cs benchmarks/csharp/Program.cs
    dotnet run --project benchmarks/csharp -c Release 2>/dev/null
fi

# Run Python
echo ""
if command -v python3 &> /dev/null; then
    python3 benchmarks/string_ops.py
fi

echo ""
echo "=========================================="
