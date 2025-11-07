#!/bin/bash

# Quadrate test runner
# Compiles and runs .qd test files, comparing output with expected results

set -u  # Exit on undefined variable

# Get script directory and source shared utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_utils.sh"

# Paths
QUADC="${QUADC:-build/debug/bin/quadc/quadc}"
TEST_DIR="tests/qd"
TEMP_DIR="/tmp/qd_tests_$$"
export QUADRATE_ROOT="lib/stdqd/qd"

# Create temp directory
mkdir -p "$TEMP_DIR"

# Cleanup on exit
trap "rm -rf $TEMP_DIR" EXIT

print_header "Quadrate Language Test Suite"

# Function to run a single test
run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .qd)
    local expected_file="${test_file%.qd}.out"
    local output_file="$TEMP_DIR/${test_name}.out"
    local binary="$TEMP_DIR/${test_name}"

    increment_test_counter

    # Check if expected output exists
    if [ ! -f "$expected_file" ]; then
        log_skip "$test_name" "no expected output file"
        return
    fi

    # Compile the test
    if ! "$QUADC" "$test_file" -o "$binary" 2>"$TEMP_DIR/${test_name}.err"; then
        log_fail "$test_name" "compilation failed"
        cat "$TEMP_DIR/${test_name}.err"
        return
    fi

    # Run the test and capture output
    if ! "$binary" > "$output_file" 2>&1; then
        log_fail "$test_name" "runtime error"
        cat "$output_file"
        return
    fi

    # Compare output with expected
    if diff -q "$expected_file" "$output_file" > /dev/null 2>&1; then
        log_pass "$test_name"
    else
        log_fail "$test_name" "output mismatch"
        print_indented "Expected" "$expected_file"
        print_indented "Got" "$output_file"
        print_diff "$expected_file" "$output_file"
    fi
}

# Function to run a negative test (should fail compilation)
run_negative_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .qd)
    local expected_error_file="${test_file%.qd}.err"
    local actual_error_file="$TEMP_DIR/${test_name}.err"
    local binary="$TEMP_DIR/${test_name}"

    increment_test_counter

    # Check if expected error file exists
    if [ ! -f "$expected_error_file" ]; then
        log_skip "$test_name" "no expected error file"
        return
    fi

    # Try to compile - should fail
    if "$QUADC" "$test_file" -o "$binary" 2>"$actual_error_file"; then
        log_fail "$test_name" "compilation succeeded (should have failed)"
        return
    fi

    # Check if error message contains expected text
    local expected_pattern=$(cat "$expected_error_file")
    if grep -qF "$expected_pattern" "$actual_error_file"; then
        log_pass "$test_name"
    else
        log_fail "$test_name" "wrong error message"
        echo "  Expected to contain: $expected_pattern"
        echo "  Actual error:"
        cat "$actual_error_file" | sed 's/^/    /'
    fi
}

# Find and run all .qd test files
# Exclude module component files (files in subdirectories of modules/)
while IFS= read -r test_file; do
    # Check if this is a negative test (has .err file instead of .out)
    if [ -f "${test_file%.qd}.err" ]; then
        run_negative_test "$test_file"
    else
        run_test "$test_file"
    fi
done < <(find "$TEST_DIR" -name "*.qd" -type f \
    ! -path "*/modules/*/*" \
    | sort)

# Print summary and exit
print_summary
print_result_and_exit
