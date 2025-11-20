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

# Run Quadrate
if [ -f benchmarks/arithmetic_qd ]; then
    benchmarks/arithmetic_qd
    echo ""
fi

# Run C
if [ -f benchmarks/arithmetic_c ]; then
    benchmarks/arithmetic_c
    echo ""
fi

# Run Rust
if [ -f benchmarks/arithmetic_rust ]; then
    benchmarks/arithmetic_rust
    echo ""
fi

# Run Go
if [ -f benchmarks/arithmetic_go ]; then
    benchmarks/arithmetic_go
    echo ""
fi

# Run Node.js
if command -v node &> /dev/null; then
    node benchmarks/arithmetic.js
    echo ""
fi

# Run Python
if command -v python3 &> /dev/null; then
    python3 benchmarks/arithmetic.py
    echo ""
fi

echo "=========================================="
echo "  Benchmark Complete"
echo "=========================================="
