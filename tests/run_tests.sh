#!/bin/bash

# Unified test runner for Quadrate
# Supports multiple test modes: qd tests (with C/LLVM backends), formatter tests, quaduses tests, and valgrind

set -u

# Get script directory and source shared utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_utils.sh"

# Parse command line arguments
MODE="${1:-qd}"  # qd, formatter, quaduses, valgrind

# Configuration
QUADC="${QUADC:-build/debug/bin/quadc/quadc}"
QUADFMT="${QUADFMT:-dist/bin/quadfmt}"
QUADUSES="${QUADUSES:-dist/bin/quaduses}"
QUADRATE_ROOT="${QUADRATE_ROOT:-}"
QUADRATE_LIBDIR="${QUADRATE_LIBDIR:-dist/lib}"
TEST_DIR_QD="tests/qd"
TEST_DIR_FORMATTER="tests/formatter"
EXPECTED_DIR_FORMATTER="tests/formatter/expected"
TEST_DIR_QUADUSES="tests/quaduses"
EXPECTED_DIR_QUADUSES="tests/quaduses/expected"
TEMP_DIR="/tmp/qd_tests_$$"

export QUADRATE_ROOT
export QUADRATE_LIBDIR

# Create temp directory
mkdir -p "$TEMP_DIR/results"

# Cleanup on exit
trap "rm -rf $TEMP_DIR" EXIT

# Function to run a single Quadrate test
run_qd_test() {
    local test_file="$1"
    local compiler="$2"
    local use_valgrind="${3:-no}"
    local opt_flags="${4:-}"
    local test_name=$(basename "$test_file" .qd)
    local test_id=$(echo "$test_file" | md5sum | cut -d' ' -f1)
    local result_file="$TEMP_DIR/results/${test_id}.result"

    echo "NAME:$test_name" > "$result_file"

    # Check if this is a negative test
    if [ -f "${test_file%.qd}.err" ]; then
        # Negative test - should fail to compile
        local expected_error_file="${test_file%.qd}.err"
        local actual_error_file="$TEMP_DIR/${test_id}.err"
        local binary="$TEMP_DIR/${test_id}"

        if "$compiler" $opt_flags "$test_file" -o "$binary" 2>"$actual_error_file" >/dev/null; then
            echo "FAIL:compilation succeeded (should have failed)" >> "$result_file"
            echo -e "\033[0;31mFAIL\033[0m  $test_name (compilation succeeded)"
            return
        fi

        # Check if all error patterns are present
        local all_patterns_found=true
        while IFS= read -r pattern; do
            if ! grep -qF "$pattern" "$actual_error_file"; then
                all_patterns_found=false
                break
            fi
        done < "$expected_error_file"

        if $all_patterns_found; then
            echo "PASS" >> "$result_file"
            echo -e "\033[0;32mPASS\033[0m  $test_name"
        else
            echo "FAIL:error message mismatch" >> "$result_file"
            echo -e "\033[0;31mFAIL\033[0m  $test_name (error message mismatch)"
        fi
        return
    fi

    # Positive test - should compile and run
    local expected_output_file="${test_file%.qd}.out"
    if [ ! -f "$expected_output_file" ]; then
        echo "SKIP:no expected output" >> "$result_file"
        echo -e "\033[1;33mSKIP\033[0m  $test_name"
        return
    fi

    local binary="$TEMP_DIR/${test_id}"
    local actual_output_file="$TEMP_DIR/${test_id}.out"
    local compile_log="$TEMP_DIR/${test_id}.compile"

    # Compile
    if ! "$compiler" $opt_flags "$test_file" -o "$binary" 2>"$compile_log" >/dev/null; then
        echo "FAIL:compilation failed" >> "$result_file"
        echo -e "\033[0;31mFAIL\033[0m  $test_name (compilation failed)"
        return
    fi

    # Run (with or without valgrind)
    if [ "$use_valgrind" = "yes" ]; then
        local valgrind_log="$TEMP_DIR/${test_id}.valgrind"
        if ! valgrind --leak-check=full --error-exitcode=1 --log-file="$valgrind_log" "$binary" >"$actual_output_file" 2>&1; then
            echo "FAIL:valgrind errors" >> "$result_file"
            echo -e "\033[0;31mFAIL\033[0m  $test_name (valgrind errors)"
            return
        fi
    else
        if ! "$binary" >"$actual_output_file" 2>&1; then
            echo "FAIL:runtime error" >> "$result_file"
            echo -e "\033[0;31mFAIL\033[0m  $test_name (runtime error)"
            return
        fi
    fi

    # Compare output
    if diff -q "$expected_output_file" "$actual_output_file" >/dev/null; then
        echo "PASS" >> "$result_file"
        echo -e "\033[0;32mPASS\033[0m  $test_name"
    else
        echo "FAIL:output mismatch" >> "$result_file"
        echo -e "\033[0;31mFAIL\033[0m  $test_name (output mismatch)"
    fi
}

# Function to run a formatter test
run_formatter_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .qd)
    local expected_file="$EXPECTED_DIR_FORMATTER/${test_name}.qd"
    local output_file="$TEMP_DIR/${test_name}.qd"

    increment_test_counter

    if [ ! -f "$expected_file" ]; then
        log_skip "$test_name" "no expected output"
        return
    fi

    # Copy input to temp and format in place
    cp "$test_file" "$output_file"
    if ! "$QUADFMT" -w "$output_file" >/dev/null 2>&1; then
        log_fail "$test_name" "formatter failed"
        return
    fi

    # Compare with expected
    if diff -q "$expected_file" "$output_file" >/dev/null; then
        log_pass "$test_name"
    else
        log_fail "$test_name" "output mismatch"
        print_diff "$expected_file" "$output_file"
    fi
}

# Function to run a quaduses test
run_quaduses_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .qd)
    local expected_file="$EXPECTED_DIR_QUADUSES/${test_name}.qd"
    local output_file="$TEMP_DIR/${test_name}.qd"

    increment_test_counter

    if [ ! -f "$expected_file" ]; then
        log_skip "$test_name" "no expected output"
        return
    fi

    # Copy input to temp and process with quaduses in place
    cp "$test_file" "$output_file"
    if ! "$QUADUSES" -w "$output_file" >/dev/null 2>&1; then
        log_fail "$test_name" "quaduses failed"
        return
    fi

    # Compare with expected
    if diff -q "$expected_file" "$output_file" >/dev/null; then
        log_pass "$test_name"
    else
        log_fail "$test_name" "output mismatch"
        print_diff "$expected_file" "$output_file"
    fi
}

# Main execution based on mode
case "$MODE" in
    qd)
        # Run Quadrate language tests
        print_header "Quadrate Language Tests"
        COMPILER="$QUADC"

        # Find and run all tests in parallel
        export -f run_qd_test
        export TEMP_DIR
        export COMPILER

        if command -v parallel &> /dev/null; then
            # Use parallel if available
            find "$TEST_DIR_QD" -name "*.qd" -type f | sort | \
                parallel --unsafe -j$(nproc) run_qd_test {} "$COMPILER" no ""
        else
            # Fallback to xargs for sequential execution
            find "$TEST_DIR_QD" -name "*.qd" -type f | sort | \
                xargs -I {} bash -c 'run_qd_test "$@"' _ {} "$COMPILER" no ""
        fi

        # Collect results
        for result_file in "$TEMP_DIR/results"/*.result; do
            [ -f "$result_file" ] || continue
            increment_test_counter

            test_name=$(grep "^NAME:" "$result_file" | cut -d: -f2-)
            status=$(grep -v "^NAME:" "$result_file" | head -1)

            case "$status" in
                PASS)
                    TESTS_PASSED=$((TESTS_PASSED + 1))
                    ;;
                SKIP*)
                    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
                    ;;
                *)
                    TESTS_FAILED=$((TESTS_FAILED + 1))
                    ;;
            esac
        done

        print_summary
        print_result_and_exit
        ;;

    formatter)
        # Run formatter tests
        print_header "Quadrate Formatter Tests"

        while IFS= read -r test_file; do
            run_formatter_test "$test_file"
        done < <(find "$TEST_DIR_FORMATTER" -name "*.qd" -type f ! -path "*/expected/*" | sort)

        print_summary
        print_result_and_exit
        ;;

    quaduses)
        # Run quaduses tests
        print_header "Quadrate Use Statement Manager Tests"

        while IFS= read -r test_file; do
            run_quaduses_test "$test_file"
        done < <(find "$TEST_DIR_QUADUSES" -name "*.qd" -type f ! -path "*/expected/*" | sort)

        print_summary
        print_result_and_exit
        ;;

    valgrind)
        # Run Quadrate tests with valgrind
        print_header "Quadrate Language Tests (with Valgrind)"

        if ! command -v valgrind >/dev/null 2>&1; then
            echo "Error: valgrind not found"
            exit 1
        fi

        # Export function for sequential execution
        export -f run_qd_test
        export TEMP_DIR
        export QUADC

        # Run tests sequentially (parallel + valgrind can be problematic)
        find "$TEST_DIR_QD" -name "*.qd" -type f | sort | while read test_file; do
            run_qd_test "$test_file" "$QUADC" yes ""
        done

        # Collect results
        for result_file in "$TEMP_DIR/results"/*.result; do
            [ -f "$result_file" ] || continue
            increment_test_counter

            test_name=$(grep "^NAME:" "$result_file" | cut -d: -f2-)
            status=$(grep -v "^NAME:" "$result_file" | head -1)

            case "$status" in
                PASS)
                    TESTS_PASSED=$((TESTS_PASSED + 1))
                    ;;
                SKIP*)
                    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
                    ;;
                *)
                    TESTS_FAILED=$((TESTS_FAILED + 1))
                    ;;
            esac
        done

        print_summary
        print_result_and_exit
        ;;

    optimized)
        # Run Quadrate tests with optimization enabled
        OPT_LEVEL="${2:--O2}"
        print_header "Quadrate Language Tests (with $OPT_LEVEL)"
        COMPILER="$QUADC"

        # Find and run all tests in parallel
        export -f run_qd_test
        export TEMP_DIR
        export COMPILER
        export OPT_LEVEL

        if command -v parallel &> /dev/null; then
            # Use parallel if available
            find "$TEST_DIR_QD" -name "*.qd" -type f | sort | \
                parallel --unsafe -j$(nproc) run_qd_test {} "$COMPILER" no "$OPT_LEVEL"
        else
            # Fallback to xargs for sequential execution
            find "$TEST_DIR_QD" -name "*.qd" -type f | sort | \
                xargs -I {} bash -c 'run_qd_test "$@"' _ {} "$COMPILER" no "$OPT_LEVEL"
        fi

        # Collect results
        for result_file in "$TEMP_DIR/results"/*.result; do
            [ -f "$result_file" ] || continue
            increment_test_counter

            test_name=$(grep "^NAME:" "$result_file" | cut -d: -f2-)
            status=$(grep -v "^NAME:" "$result_file" | head -1)

            case "$status" in
                PASS)
                    TESTS_PASSED=$((TESTS_PASSED + 1))
                    ;;
                SKIP*)
                    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
                    ;;
                *)
                    TESTS_FAILED=$((TESTS_FAILED + 1))
                    ;;
            esac
        done

        print_summary
        print_result_and_exit
        ;;

    *)
        echo "Usage: $0 [qd|formatter|quaduses|valgrind|optimized] [optimization_level]"
        echo ""
        echo "Modes:"
        echo "  qd         - Run Quadrate language tests (default)"
        echo "  formatter  - Run formatter tests"
        echo "  quaduses   - Run use statement manager tests"
        echo "  valgrind   - Run Quadrate tests with valgrind"
        echo "  optimized  - Run Quadrate tests with optimization (default: -O2)"
        echo ""
        echo "Examples:"
        echo "  $0 optimized          # Run with -O2 optimization"
        echo "  $0 optimized -O3      # Run with -O3 optimization"
        echo ""
        echo "Environment variables:"
        echo "  QUADC            - Path to quadc compiler"
        echo "  QUADFMT          - Path to quadfmt formatter"
        echo "  QUADUSES         - Path to quaduses tool"
        echo "  QUADRATE_ROOT    - Path to standard library"
        echo "  QUADRATE_LIBDIR  - Path to libraries"
        exit 1
        ;;
esac
