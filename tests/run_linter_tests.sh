#!/bin/bash

# Test runner for quadlint (Quadrate linter)
# Tests linter functionality by comparing actual output to expected output

set -u

# Get script directory and source shared utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_utils.sh"

# Configuration
QUADLINT="${QUADLINT:-dist/bin/quadlint}"
TEST_DIR="tests/linter"
TEMP_DIR="/tmp/quadlint_tests_$$"

# Create temp directory
mkdir -p "$TEMP_DIR/results"

# Cleanup on exit
trap "rm -rf $TEMP_DIR" EXIT

# Function to run a single linter test
run_linter_test() {
    local test_file="$1"
    local options="${2:-}"
    local test_name=$(basename "$test_file" .qd)
    local test_dir=$(dirname "$test_file")
    local test_id=$(echo "$test_file$options" | md5sum | cut -d' ' -f1)
    local result_file="$TEMP_DIR/results/${test_id}.result"

    echo "NAME:$test_name" > "$result_file"

    # Determine expected output file
    local expected_file
    if [ -n "$options" ]; then
        # For tests with options, use a specific expected file
        local option_name=$(echo "$options" | sed 's/^--//' | sed 's/-/_/g' | sed 's/ /_/g')
        expected_file="${test_file%.qd}_${option_name}.out"
        if [ ! -f "$expected_file" ]; then
            expected_file="${test_dir}/${option_name}.out"
        fi
    else
        expected_file="${test_file%.qd}.out"
    fi

    if [ ! -f "$expected_file" ]; then
        echo "RESULT:SKIP" >> "$result_file"
        echo "REASON:No expected output file: $expected_file" >> "$result_file"
        return
    fi

    # Run linter and capture output (both stdout and stderr)
    local actual_output="$TEMP_DIR/${test_id}.actual"
    if [ -n "$options" ]; then
        $QUADLINT $options "$test_file" > "$actual_output" 2>&1 || true
    else
        $QUADLINT "$test_file" > "$actual_output" 2>&1 || true
    fi

    # Compare output
    if diff -u "$expected_file" "$actual_output" > "$TEMP_DIR/${test_id}.diff" 2>&1; then
        echo "RESULT:PASS" >> "$result_file"
    else
        echo "RESULT:FAIL" >> "$result_file"
        echo "DIFF:" >> "$result_file"
        cat "$TEMP_DIR/${test_id}.diff" >> "$result_file"
    fi
}

# Find all test files
find_tests() {
    find "$TEST_DIR" -name "*.qd" | sort
}

# Main execution
main() {
    local total=0
    local passed=0
    local failed=0
    local skipped=0

    echo "Running quadlint tests..."
    echo

    # Run all tests
    while IFS= read -r test_file; do
        total=$((total + 1))

        # Run test without options
        run_linter_test "$test_file" ""

        # Check for special option-based tests
        local test_dir=$(dirname "$test_file")
        if [ "$test_dir" = "tests/linter/options" ]; then
            # Run with --no-unused-functions if expected file exists
            if [ -f "${test_dir}/no_unused_functions.out" ]; then
                total=$((total + 1))
                run_linter_test "$test_file" "--no-unused-functions"
            fi
        fi
    done < <(find_tests)

    # Collect results
    for result_file in "$TEMP_DIR/results"/*.result; do
        [ -f "$result_file" ] || continue

        local test_name=$(grep "^NAME:" "$result_file" | cut -d: -f2-)
        local result=$(grep "^RESULT:" "$result_file" | cut -d: -f2)

        case "$result" in
            PASS)
                passed=$((passed + 1))
                log_pass "$test_name"
                ;;
            FAIL)
                failed=$((failed + 1))
                log_fail "$test_name" ""
                # Show diff
                sed -n '/^DIFF:/,$ { /^DIFF:/d; p }' "$result_file" | sed 's/^/  /'
                ;;
            SKIP)
                skipped=$((skipped + 1))
                local reason=$(grep "^REASON:" "$result_file" | cut -d: -f2-)
                log_skip "$test_name" "$reason"
                ;;
        esac
    done

    # Print summary
    echo
    echo "================================"
    echo "Test Summary"
    echo "================================"
    echo "Total:   $total"
    echo "Passed:  $passed"
    echo "Failed:  $failed"
    echo "Skipped: $skipped"
    echo "================================"

    if [ $failed -gt 0 ]; then
        exit 1
    fi
}

main
