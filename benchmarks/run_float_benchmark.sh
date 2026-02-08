#!/bin/bash

echo "=========================================="
echo "  Float Compute Performance Benchmark"
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
echo "Compiling benchmarks/float_compute.qd..."
$QUADC benchmarks/float_compute.qd -O3 -o benchmarks/float_compute_qd
if [ $? -eq 0 ]; then
    echo ""
    ./benchmarks/float_compute_qd
fi

# Compile and Run C
echo ""
echo "Compiling C float benchmark..."
gcc -O3 benchmarks/float_compute.c -o benchmarks/float_compute_c -lm
if [ $? -eq 0 ]; then
    ./benchmarks/float_compute_c
fi

# Compile and Run Rust
echo ""
echo "Compiling Rust float benchmark..."
rustc -O benchmarks/float_compute.rs -o benchmarks/float_compute_rust 2>/dev/null
if [ $? -eq 0 ]; then
    ./benchmarks/float_compute_rust
fi

# Compile and Run Go
echo ""
echo "Compiling Go float benchmark..."
go build -o benchmarks/float_compute_go benchmarks/float_compute.go 2>/dev/null
if [ $? -eq 0 ]; then
    ./benchmarks/float_compute_go
fi

# Run Node.js
echo ""
if command -v node &> /dev/null; then
    node benchmarks/float_compute.js
fi

# Run Python
echo ""
if command -v python3 &> /dev/null; then
    python3 benchmarks/float_compute.py
fi

echo ""
echo "=========================================="
