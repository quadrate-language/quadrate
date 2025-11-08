#!/bin/bash

# Parallel Quadrate test runner
# Runs tests in parallel to minimize testing time

set -u

# Get script directory and source shared utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_utils.sh"

# Paths
QUADC="${QUADC:-build/debug/bin/quadc/quadc}"
TEST_DIR="tests/qd"
TEMP_DIR="/tmp/qd_tests_$$"
export QUADRATE_ROOT="lib/stdqd/qd"

# Create temp directory
mkdir -p "$TEMP_DIR/results"

# Cleanup on exit
trap "rm -rf $TEMP_DIR" EXIT

print_header "Quadrate Language Test Suite (Parallel)"

# Function to run a single test (will be called in parallel)
run_single_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .qd)
    # Use full path hash to avoid collisions for tests with same basename
    local test_id=$(echo "$test_file" | md5sum | cut -d' ' -f1)
    local result_file="$TEMP_DIR/results/${test_id}.result"
    # Store test name in result for later display
    echo "NAME:$test_name" > "$result_file"

    # Check if this is a negative test
    if [ -f "${test_file%.qd}.err" ]; then
        # Negative test
        local expected_error_file="${test_file%.qd}.err"
        local actual_error_file="$TEMP_DIR/${test_id}.err"
        local binary="$TEMP_DIR/${test_id}"

        # Try to compile - should fail
        if "$QUADC" "$test_file" -o "$binary" 2>"$actual_error_file" >/dev/null; then
            echo "FAIL:compilation succeeded (should have failed)" >> "$result_file"
            echo -e "\033[0;31mFAIL\033[0m  $test_name (compilation succeeded)"
            return
        fi

        # Check if all error patterns are present
        local all_found=true
        while IFS= read -r pattern; do
            [ -z "$pattern" ] && continue
            if ! grep -qF "$pattern" "$actual_error_file"; then
                all_found=false
                break
            fi
        done < "$expected_error_file"

        if $all_found; then
            echo "PASS" >> "$result_file"
            echo -e "\033[0;32mPASS\033[0m  $test_name"
        else
            echo "FAIL:wrong error message" >> "$result_file"
            echo -e "\033[0;31mFAIL\033[0m  $test_name (wrong error message)"
        fi
    else
        # Positive test
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

        # Run the test and capture output
        if ! "$binary" > "$output_file" 2>&1; then
            echo "FAIL:runtime error" >> "$result_file"
            echo -e "\033[0;31mFAIL\033[0m  $test_name (runtime error)"
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
    fi
}

export -f run_single_test
export QUADC TEMP_DIR

# Find all test files and run them in parallel
# Use number of CPU cores for parallelism
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
