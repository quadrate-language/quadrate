#!/bin/bash

# Parallel Quadrate test runner with valgrind
# Checks compiled programs for memory leaks in parallel

set -u

# Get script directory and source shared utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_utils.sh"

# Paths
QUADC="${QUADC:-build/debug/bin/quadc/quadc}"
TEST_DIR="tests/qd"
TEMP_DIR="/tmp/qd_tests_valgrind_$$"
export QUADRATE_ROOT="lib/stdqd/qd"

# Valgrind options
VALGRIND_SUPP="${SCRIPT_DIR}/valgrind.supp"
if [ -f "$VALGRIND_SUPP" ]; then
	VALGRIND_OPTS="--leak-check=full --error-exitcode=1 --quiet --suppressions=$VALGRIND_SUPP"
else
	VALGRIND_OPTS="--leak-check=full --error-exitcode=1 --quiet"
fi

# Create temp directory
mkdir -p "$TEMP_DIR/results"

# Cleanup on exit
trap "rm -rf $TEMP_DIR" EXIT

print_header "Quadrate Language Test Suite (Valgrind Parallel)"

# Function to run a single test with valgrind (called in parallel)
run_single_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .qd)
    local test_id=$(echo "$test_file" | md5sum | cut -d' ' -f1)
    local result_file="$TEMP_DIR/results/${test_id}.result"

    # Store test name
    echo "NAME:$test_name" > "$result_file"

    # Skip negative tests
    if [ -f "${test_file%.qd}.err" ]; then
        echo "SKIP:negative test" >> "$result_file"
        echo -e "\033[1;33mSKIP\033[0m  $test_name (negative test)"
        return
    fi

    local expected_file="${test_file%.qd}.out"
    local output_file="$TEMP_DIR/${test_id}.out"
    local binary="$TEMP_DIR/${test_id}"

    # Check if expected output exists
    if [ ! -f "$expected_file" ]; then
        echo "SKIP:no expected output file" >> "$result_file"
        echo -e "\033[1;33mSKIP\033[0m  $test_name (no expected output)"
        return
    fi

    # Compile the test
    if ! "$QUADC" "$test_file" -o "$binary" 2>"$TEMP_DIR/${test_id}.err" >/dev/null; then
        echo "FAIL:compilation failed" >> "$result_file"
        echo -e "\033[0;31mFAIL\033[0m  $test_name (compilation failed)"
        return
    fi

    # Run with valgrind
    if ! valgrind $VALGRIND_OPTS "$binary" > "$output_file" 2>"$TEMP_DIR/${test_id}.valgrind"; then
        echo "FAIL:memory leak detected" >> "$result_file"
        echo -e "\033[0;31mFAIL\033[0m  $test_name (memory leak detected)"
        return
    fi

    # Compare output with expected
    if diff -q "$expected_file" "$output_file" > /dev/null 2>&1; then
        echo "PASS" >> "$result_file"
        echo -e "\033[0;32mPASS\033[0m  $test_name"
    else
        echo "FAIL:output mismatch" >> "$result_file"
        echo -e "\033[0;31mFAIL\033[0m  $test_name (output mismatch)"
    fi
}

export -f run_single_test
export QUADC TEMP_DIR VALGRIND_OPTS QUADRATE_ROOT

# Find all test files and run them in parallel
NPROC=$(nproc 2>/dev/null || echo 4)

find "$TEST_DIR" -name "*.qd" -type f ! -path "*/modules/*/*" | sort | \
    xargs -P "$NPROC" -I {} bash -c 'run_single_test "$@"' _ {}

# Collect results (just count, already printed)
total=0
passed=0
failed=0
skipped=0

for result_file in "$TEMP_DIR/results/"*.result; do
    [ -f "$result_file" ] || continue

    result=$(tail -n1 "$result_file")
    total=$((total + 1))

    case "$result" in
        PASS)
            passed=$((passed + 1))
            ;;
        SKIP:*)
            skipped=$((skipped + 1))
            ;;
        FAIL:*)
            failed=$((failed + 1))
            ;;
    esac
done

echo ""
echo "================================================"
echo "  Test Summary"
echo "================================================"
echo "Tests run:    $total"
printf "Tests passed: "
if [ "$passed" -eq "$total" ]; then
    echo -e "\033[0;32m$passed\033[0m"
else
    echo "$passed"
fi
printf "Tests failed: "
if [ "$failed" -gt 0 ]; then
    echo -e "\033[0;31m$failed\033[0m"
else
    echo "$failed"
fi

echo ""
if [ "$failed" -eq 0 ]; then
    echo -e "\033[0;32mAll tests passed!\033[0m"
    exit 0
else
    echo -e "\033[0;31m$failed test(s) failed\033[0m"
    exit 1
fi
