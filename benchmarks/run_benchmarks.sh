#!/bin/bash

echo "=========================================="
echo "  Language Performance Benchmarks"
echo "=========================================="
echo ""
echo "Running benchmarks for:"
echo "  - Arithmetic loop (10M iterations)"
echo "  - Recursive Fibonacci (n=35)"
echo ""
echo "=========================================="
echo ""

QUADC="dist/bin/quadc"

if [ ! -f "$QUADC" ]; then
    echo "Error: quadc not found in dist/bin. Run 'make release' first."
    exit 1
fi

export QUADRATE_LIBDIR="dist/lib"
if [ "$(uname -s)" = "Haiku" ]; then
    export QUADRATE_ROOT="dist/data/quadrate"
else
    export QUADRATE_ROOT="dist/share/quadrate"
fi
export LD_LIBRARY_PATH="dist/lib:$LD_LIBRARY_PATH"

# Compile and Run Quadrate
echo "Compiling benchmarks/arithmetic.qd..."
$QUADC benchmarks/arithmetic.qd -O3 -o benchmarks/arithmetic_qd
if [ $? -eq 0 ]; then
    benchmarks/arithmetic_qd
    echo ""
fi

# Compile and Run C
echo "Compiling C benchmark..."
gcc -O3 benchmarks/arithmetic.c -o benchmarks/arithmetic_c
if [ $? -eq 0 ]; then
    benchmarks/arithmetic_c
    echo ""
fi

# Compile and Run Rust
echo "Compiling Rust benchmark..."
rustc -O benchmarks/arithmetic.rs -o benchmarks/arithmetic_rust 2>/dev/null
if [ $? -eq 0 ]; then
    benchmarks/arithmetic_rust
    echo ""
fi

# Compile and Run Go
echo "Compiling Go benchmark..."
go build -o benchmarks/arithmetic_go benchmarks/arithmetic.go 2>/dev/null
if [ $? -eq 0 ]; then
    benchmarks/arithmetic_go
    echo ""
fi

# Run Node.js
if command -v node &> /dev/null; then
    node benchmarks/arithmetic.js
    echo ""
fi

# Run C#
if command -v dotnet &> /dev/null; then
    cp benchmarks/arithmetic.cs benchmarks/csharp/Program.cs
    dotnet run --project benchmarks/csharp -c Release 2>/dev/null
    echo ""
fi

# Run Python
if command -v python3 &> /dev/null; then
    python3 benchmarks/arithmetic.py
    echo ""
fi

# Run String Benchmarks
if [ -f benchmarks/run_string_benchmark.sh ]; then
    ./benchmarks/run_string_benchmark.sh
    echo ""
fi

# Run Compute Benchmarks
if [ -f benchmarks/run_compute_benchmark.sh ]; then
    ./benchmarks/run_compute_benchmark.sh
    echo ""
fi

# Run Float Compute Benchmarks
if [ -f benchmarks/run_float_benchmark.sh ]; then
    ./benchmarks/run_float_benchmark.sh
    echo ""
fi

echo "=========================================="
echo "  Benchmark Complete"
echo "=========================================="
