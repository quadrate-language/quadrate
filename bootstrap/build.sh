#!/bin/bash
# Bootstrap build script for the Quadrate self-hosted compiler.
# Compiles the portable LLVM IR into a native binary.
#
# Usage: bash bootstrap/build.sh [output_path]
#
# Requires: clang, Quadrate runtime libraries in dist/lib/

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
OUTPUT="${1:-$PROJECT_DIR/dist/bin/quadc-bootstrap}"
LIBDIR="${QUADRATE_LIBDIR:-$PROJECT_DIR/dist/lib}"

# Core libraries needed by the self-hosted compiler
LIBS="-lqdrt -lm -lstdc++"
# Module libraries used by the compiler
LIBS="$LIBS -lqdio -lqdos -lqdstr -lqdstrconv -lqdmem"

echo "Compiling bootstrap IR -> $OUTPUT"
clang "$SCRIPT_DIR/quadc.ll" \
    -L"$LIBDIR" \
    $LIBS \
    -o "$OUTPUT" \
    -Wno-override-module

echo "Bootstrap compiler ready: $OUTPUT"
echo "Usage: QUADRATE_LIBDIR=$LIBDIR QUADRATE_ROOT=$PROJECT_DIR/dist/share/quadrate QUADC_INPUT=file.qd QUADC_OUTPUT=out $OUTPUT"
