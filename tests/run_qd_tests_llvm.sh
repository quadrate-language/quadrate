#!/bin/bash

# Quadrate Language Tests for LLVM Backend
# Usage: QUADC_LLVM=path/to/quadc-llvm QUADRATE_ROOT=path/to/stdqd bash run_qd_tests_llvm.sh

QUADC_LLVM="${QUADC_LLVM:-../../build/debug/bin/quadc-llvm/quadc-llvm}"
QUADRATE_ROOT="${QUADRATE_ROOT:-../../lib/stdqd/qd}"

# Convert to absolute paths
QUADC_LLVM="$(cd "$(dirname "$QUADC_LLVM")" && pwd)/$(basename "$QUADC_LLVM")"
QUADRATE_ROOT="$(cd "$QUADRATE_ROOT" && pwd)"

export QUADRATE_ROOT

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "================================================"
echo "  Quadrate Language Test Suite (LLVM Backend)"
echo "================================================"
echo ""
echo "Compiler: $QUADC_LLVM"
echo "Stdlib:   $QUADRATE_ROOT"
echo ""

total_passed=0
total_tests=0
total_skipped=0

# Test categories
for cat in arithmetic control_flow documentation errors imports io logic modules stack strings threading; do
    passed=0
    tests=0

    # modules and imports need to run from their directory (relative module paths)
    if [ "$cat" = "modules" ] || [ "$cat" = "imports" ]; then
        cd "$cat"
        for f in *.qd; do
            if [ -f "${f%.qd}.out" ]; then
                tests=$((tests + 1))
                if timeout 5 "$QUADC_LLVM" "$f" -r 2>&1 | sed -n '/=== Running/,$ {/=== Running/d; p}' | diff -q "${f%.qd}.out" - >/dev/null 2>&1; then
                    passed=$((passed + 1))
                    echo -e "${GREEN}PASS${NC}  $f"
                else
                    echo -e "${RED}FAIL${NC}  $f (output mismatch)"
                fi
            fi
        done
        cd ..
    else
        for f in $cat/*.qd; do
            if [ -f "${f%.qd}.out" ]; then
                tests=$((tests + 1))
                if timeout 2 "$QUADC_LLVM" "$f" -r 2>&1 | sed -n '/=== Running/,$ {/=== Running/d; p}' | diff -q "${f%.qd}.out" - >/dev/null 2>&1; then
                    passed=$((passed + 1))
                    echo -e "${GREEN}PASS${NC}  $(basename $f)"
                else
                    echo -e "${RED}FAIL${NC}  $(basename $f) (output mismatch)"
                fi
            fi
        done
    fi
    
    echo ""
    echo "$cat: $passed/$tests"
    total_passed=$((total_passed + passed))
    total_tests=$((total_tests + tests))
done

echo ""
echo "================================================"
echo "  Test Summary (LLVM Backend)"
echo "================================================"
echo "Tests run:    $total_tests"
echo "Tests passed: $total_passed"
failed=$((total_tests - total_passed))
if [ $failed -gt 0 ]; then
    echo -e "Tests failed: ${RED}$failed${NC}"
else
    echo "Tests failed: 0"
fi
echo ""

# Calculate percentage
if [ $total_tests -gt 0 ]; then
    percentage=$(echo "scale=1; $total_passed * 100 / $total_tests" | bc)
    echo "Pass rate: ${percentage}%"
    echo ""
fi

# Exit with failure if any tests failed
if [ $failed -gt 0 ]; then
    exit 1
fi
