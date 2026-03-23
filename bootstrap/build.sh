#!/bin/bash
# Bootstrap build script for the Quadrate self-hosted compiler.
# Compiles the portable LLVM IR into a native binary.
#
# Usage: bash bootstrap/build.sh [output_path]
#
# Requires: clang, Quadrate runtime libraries in dist/lib/

set -e
ulimit -s unlimited 2>/dev/null || true

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
OUTPUT="${1:-$PROJECT_DIR/dist/bin/quadc-bootstrap}"
LIBDIR="${QUADRATE_LIBDIR:-$PROJECT_DIR/dist/lib}"

# Core and module libraries needed by the self-hosted compiler
LIBS="-lrt -lm -lstdc++ -lio -los -lstrings -lstrconv -lmem -lllvmwrap -lLLVM-22"

echo "Compiling bootstrap IR -> $OUTPUT"
clang "$SCRIPT_DIR/quadc.ll" \
    -L"$LIBDIR" -L"$LIBDIR/quadrate" \
    $LIBS \
    -o "$OUTPUT" \
    -Wno-override-module

echo "Bootstrap compiler ready: $OUTPUT"
echo "Usage: QUADRATE_LIBDIR=$LIBDIR QUADRATE_ROOT=$PROJECT_DIR/dist/share/quadrate QUADC_INPUT=file.qd QUADC_OUTPUT=out $OUTPUT"
