#!/bin/bash
# 3-stage bootstrap verification for the Quadrate self-hosted compiler.
#
# Stage 0: quadc.ll -> native binary (quadc-stage0)
# Stage 1: quadc-stage0 compiles quadc.qd -> stage1.ll
# Stage 2: stage1.ll -> native binary (quadc-stage1) compiles quadc.qd -> stage2.ll
# Verify: stage1.ll == stage2.ll
#
# Usage: bash bootstrap/verify.sh
# Exit code 0 = bootstrap verified (fixed point reached)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
LIBDIR="${QUADRATE_LIBDIR:-$PROJECT_DIR/dist/lib}"
QUADROOT="${QUADRATE_ROOT:-$PROJECT_DIR/dist/share/quadrate}"
SRCDIR="$PROJECT_DIR/lib/qdlexer/qd/lexer"
TMPDIR=$(mktemp -d)

trap "rm -rf $TMPDIR" EXIT

LIBS="-lqdrt -lm -lstdc++ -lqdio -lqdos -lqdstr -lqdstrconv -lqdmem"

echo "=== Quadrate 3-Stage Bootstrap Verification ==="
echo ""

# Stage 0: Compile checked-in IR -> native binary
echo "[Stage 0] Compiling bootstrap/quadc.ll -> quadc-stage0"
clang "$SCRIPT_DIR/quadc.ll" -L"$LIBDIR" $LIBS -o "$TMPDIR/quadc-stage0" -Wno-override-module
echo "  OK"

# Stage 1: Use stage0 to compile quadc.qd -> stage1.ll
echo "[Stage 1] quadc-stage0 compiles quadc.qd -> stage1.ll"
QUADRATE_LIBDIR="$LIBDIR" QUADRATE_ROOT="$QUADROOT" \
    QUADC_INPUT="$SRCDIR/quadc.qd" QUADC_OUTPUT="$TMPDIR/quadc-stage1-bin" \
    "$TMPDIR/quadc-stage0" 2>/dev/null || true
# The IR is left in /tmp/quadc_output.ll by the self-hosted compiler
cp /tmp/quadc_output.ll "$TMPDIR/stage1.ll" 2>/dev/null || {
    echo "  FAIL: stage1.ll not generated"
    exit 1
}
echo "  OK ($(wc -l < "$TMPDIR/stage1.ll") lines)"

# Stage 2: Compile stage1.ll -> native, use it to compile quadc.qd -> stage2.ll
echo "[Stage 2] Compiling stage1.ll -> quadc-stage1"
clang "$TMPDIR/stage1.ll" -L"$LIBDIR" $LIBS -o "$TMPDIR/quadc-stage1" -Wno-override-module 2>/dev/null || {
    echo "  FAIL: stage1.ll failed to compile"
    exit 1
}
echo "  OK"

echo "[Stage 2] quadc-stage1 compiles quadc.qd -> stage2.ll"
QUADRATE_LIBDIR="$LIBDIR" QUADRATE_ROOT="$QUADROOT" \
    QUADC_INPUT="$SRCDIR/quadc.qd" QUADC_OUTPUT="$TMPDIR/quadc-stage2-bin" \
    "$TMPDIR/quadc-stage1" 2>/dev/null || true
cp /tmp/quadc_output.ll "$TMPDIR/stage2.ll" 2>/dev/null || {
    echo "  FAIL: stage2.ll not generated"
    exit 1
}
echo "  OK ($(wc -l < "$TMPDIR/stage2.ll") lines)"

# Verify: stage1.ll == stage2.ll
echo ""
echo "[Verify] Comparing stage1.ll and stage2.ll..."
if diff -q "$TMPDIR/stage1.ll" "$TMPDIR/stage2.ll" > /dev/null 2>&1; then
    echo ""
    echo "=== BOOTSTRAP VERIFIED ==="
    echo "Fixed point reached: stage1.ll == stage2.ll"
    echo "The self-hosted compiler reproduces itself."
    exit 0
else
    echo ""
    echo "=== BOOTSTRAP MISMATCH ==="
    echo "stage1.ll and stage2.ll differ."
    echo "Differences:"
    diff "$TMPDIR/stage1.ll" "$TMPDIR/stage2.ll" | head -20
    exit 1
fi
