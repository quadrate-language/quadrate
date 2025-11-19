#!/bin/bash

# Test runner for embed examples
# Verifies that all embed examples execute correctly
# Usage: run_embed_tests.sh [valgrind]

set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Check if running with valgrind
USE_VALGRIND="${1:-no}"
VALGRIND_CMD=""
if [ "$USE_VALGRIND" = "valgrind" ]; then
    VALGRIND_CMD="valgrind --leak-check=full --error-exitcode=1 --quiet"
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Track results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Function to run a test
run_test() {
    local test_name="$1"
    local test_exe="$2"
    local expected_pattern="$3"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    echo -n "Testing $test_name... "

    # Run the test and capture output
    if output=$($VALGRIND_CMD "$test_exe" 2>&1); then
        # Check if output matches expected pattern
        if echo "$output" | grep -q "$expected_pattern"; then
            echo -e "${GREEN}PASS${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
            return 0
        else
            echo -e "${RED}FAIL${NC} (unexpected output)"
            echo "  Expected pattern: $expected_pattern"
            echo "  Got output:"
            echo "$output" | sed 's/^/    /'
            FAILED_TESTS=$((FAILED_TESTS + 1))
            return 1
        fi
    else
        echo -e "${RED}FAIL${NC} (execution error)"
        echo "  Output:"
        echo "$output" | sed 's/^/    /'
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
}

# Main test execution
if [ "$USE_VALGRIND" = "valgrind" ]; then
    echo "=== Running Embed Examples Tests (with valgrind) ==="
else
    echo "=== Running Embed Examples Tests ==="
fi
echo ""

# Test 1: Basic embed example
run_test "embed" \
    "$PROJECT_ROOT/dist/examples/embed" \
    "Hello, World!"

# Test 2: Multi-module test
run_test "multi_module_test" \
    "$PROJECT_ROOT/dist/examples/multi_module_test" \
    "Math module:"

# Test 3: Native functions test
run_test "native_functions_test" \
    "$PROJECT_ROOT/dist/examples/native_functions_test" \
    "Current timestamp:"

# Test 4: Incremental test
run_test "incremental_test" \
    "$PROJECT_ROOT/dist/examples/incremental_test" \
    "Building all at once"

echo ""
echo "=== Test Results ==="
echo "Total:  $TOTAL_TESTS"
echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed: ${RED}$FAILED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo ""
    echo -e "${GREEN}All embed tests passed!${NC}"
    exit 0
else
    echo ""
    echo -e "${RED}Some embed tests failed!${NC}"
    exit 1
fi
