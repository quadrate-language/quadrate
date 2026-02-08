#!/bin/bash

echo "=========================================="
echo "  Compute Performance Benchmark"
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
echo "Compiling benchmarks/compute.qd..."
$QUADC benchmarks/compute.qd -O3 -o benchmarks/compute_qd
if [ $? -eq 0 ]; then
    echo ""
    ./benchmarks/compute_qd
fi

# Compile and Run C
echo ""
echo "Compiling C benchmark..."
gcc -O3 benchmarks/compute.c -o benchmarks/compute_c
if [ $? -eq 0 ]; then
    ./benchmarks/compute_c
fi

# Compile and Run Rust
echo ""
echo "Compiling Rust benchmark..."
rustc -O benchmarks/compute.rs -o benchmarks/compute_rust 2>/dev/null
if [ $? -eq 0 ]; then
    ./benchmarks/compute_rust
fi

# Compile and Run Go
echo ""
echo "Compiling Go benchmark..."
go build -o benchmarks/compute_go benchmarks/compute.go 2>/dev/null
if [ $? -eq 0 ]; then
    ./benchmarks/compute_go
fi

# Run Node.js
echo ""
if command -v node &> /dev/null; then
    node --stack-size=65536 benchmarks/compute.js
fi

# Run C#
echo ""
if command -v dotnet &> /dev/null; then
    echo "Compiling C# benchmark..."
    cp benchmarks/compute.cs benchmarks/csharp/Program.cs
    dotnet run --project benchmarks/csharp -c Release 2>/dev/null
fi

# Run Python
echo ""
if command -v python3 &> /dev/null; then
    python3 benchmarks/compute.py
fi

echo ""
echo "=========================================="
