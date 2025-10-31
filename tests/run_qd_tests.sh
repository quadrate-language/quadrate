#!/bin/bash

# Quadrate test runner
# Compiles and runs .qd test files, comparing output with expected results

set -u  # Exit on undefined variable

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Paths
QUADC="${QUADC:-build/debug/bin/quadc/quadc}"
TEST_DIR="tests/qd"
EXPECTED_DIR="tests/expected"
TEMP_DIR="/tmp/qd_tests_$$"

# Counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Create temp directory
mkdir -p "$TEMP_DIR"

# Cleanup on exit
trap "rm -rf $TEMP_DIR" EXIT

echo "================================================"
echo "  Quadrate Language Test Suite"
echo "================================================"
echo ""

# Function to run a single test
run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .qd)
    local expected_file="$EXPECTED_DIR/${test_name}.out"
    local output_file="$TEMP_DIR/${test_name}.out"
    local binary="$TEMP_DIR/${test_name}"

    TESTS_RUN=$((TESTS_RUN + 1))

    # Check if expected output exists
    if [ ! -f "$expected_file" ]; then
        echo -e "${YELLOW}SKIP${NC}  $test_name (no expected output file)"
        return
    fi

    # Compile the test
    if ! "$QUADC" "$test_file" -o "$binary" 2>"$TEMP_DIR/${test_name}.err"; then
        echo -e "${RED}FAIL${NC}  $test_name (compilation failed)"
        cat "$TEMP_DIR/${test_name}.err"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return
    fi

    # Run the test and capture output
    if ! "$binary" > "$output_file" 2>&1; then
        echo -e "${RED}FAIL${NC}  $test_name (runtime error)"
        cat "$output_file"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return
    fi

    # Compare output with expected
    if diff -q "$expected_file" "$output_file" > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}  $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAIL${NC}  $test_name (output mismatch)"
        echo "  Expected:"
        sed 's/^/    /' "$expected_file"
        echo "  Got:"
        sed 's/^/    /' "$output_file"
        echo "  Diff:"
        diff -u "$expected_file" "$output_file" | sed 's/^/    /'
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Find and run all .qd test files
while IFS= read -r test_file; do
    run_test "$test_file"
done < <(find "$TEST_DIR" -name "*.qd" -type f | sort)

# Print summary
echo ""
echo "================================================"
echo "  Test Summary"
echo "================================================"
echo "Tests run:    $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
echo ""

# Exit with appropriate code
if [ "$TESTS_FAILED" -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi
