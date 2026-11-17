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

RUN_ONE="$(dirname "$0")/run_one.sh"

"$RUN_ONE" arithmetic "Arithmetic Performance Benchmark" || exit 1
echo ""
"$RUN_ONE" string_ops "String Performance Benchmark" || exit 1
echo ""
"$RUN_ONE" compute "Compute Performance Benchmark" "" "--stack-size=65536" || exit 1
echo ""
"$RUN_ONE" float_compute "Float Compute Performance Benchmark" "-lm" || exit 1
echo ""
"$RUN_ONE" mat4x4 "Mat4x4 Multiply Performance Benchmark" || exit 1
echo ""

echo "=========================================="
echo "  Benchmark Complete"
echo "=========================================="
