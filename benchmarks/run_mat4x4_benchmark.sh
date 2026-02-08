#!/bin/bash

echo "=========================================="
echo "  Mat4x4 Multiply Performance Benchmark"
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
echo "Compiling benchmarks/mat4x4.qd..."
$QUADC benchmarks/mat4x4.qd -O3 -o benchmarks/mat4x4_qd
if [ $? -eq 0 ]; then
    echo ""
    ./benchmarks/mat4x4_qd
fi

# Compile and Run C
echo ""
echo "Compiling C benchmark..."
gcc -O3 benchmarks/mat4x4.c -o benchmarks/mat4x4_c
if [ $? -eq 0 ]; then
    ./benchmarks/mat4x4_c
fi

# Compile and Run Rust
echo ""
echo "Compiling Rust benchmark..."
rustc -O benchmarks/mat4x4.rs -o benchmarks/mat4x4_rust 2>/dev/null
if [ $? -eq 0 ]; then
    ./benchmarks/mat4x4_rust
fi

# Compile and Run Go
echo ""
echo "Compiling Go benchmark..."
go build -o benchmarks/mat4x4_go benchmarks/mat4x4.go 2>/dev/null
if [ $? -eq 0 ]; then
    ./benchmarks/mat4x4_go
fi

# Run Node.js
echo ""
if command -v node &> /dev/null; then
    node benchmarks/mat4x4.js
fi

# Run C#
echo ""
if command -v dotnet &> /dev/null; then
    echo "Compiling C# benchmark..."
    cp benchmarks/mat4x4.cs benchmarks/csharp/Program.cs
    dotnet run --project benchmarks/csharp -c Release 2>/dev/null
fi

# Run Python
echo ""
if command -v python3 &> /dev/null; then
    python3 benchmarks/mat4x4.py
fi

echo ""
echo "=========================================="
