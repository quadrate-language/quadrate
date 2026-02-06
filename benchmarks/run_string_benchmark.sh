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

# Compile
echo "Compiling benchmarks/string_ops.qd..."
$QUADC benchmarks/string_ops.qd -o benchmarks/string_ops

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# Run Quadrate
echo ""
echo "Running Quadrate..."
./benchmarks/string_ops

# Compile and Run C
echo ""
echo "Compiling C benchmark..."
gcc -O3 benchmarks/string_ops.c -o benchmarks/string_ops_c
if [ $? -eq 0 ]; then
    echo "Running C..."
    ./benchmarks/string_ops_c
fi

# Run Python
echo ""
echo "Running Python..."
if command -v python3 &> /dev/null; then
    python3 benchmarks/string_ops.py
fi

echo ""
echo "=========================================="
