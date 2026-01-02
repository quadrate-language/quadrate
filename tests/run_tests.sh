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
QUADC="${QUADC:-build/debug/cmd/quadc/quadc}"
QUADFMT="${QUADFMT:-dist/bin/quadfmt}"
QUADUSES="${QUADUSES:-dist/bin/quaduses}"
QUADRATE_ROOT="${QUADRATE_ROOT:-dist/share/quadrate}"
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
mkdir -p "$TEMP_DIR/external_modules"

# Cleanup on exit
trap "rm -rf $TEMP_DIR" EXIT

# Install external modules using quadpm (if external_modules.txt exists)
EXTERNAL_MODULES_FILE="$SCRIPT_DIR/external_modules.txt"
EXTERNAL_MODULES_PATHS_FILE="$TEMP_DIR/external_module_paths.txt"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
QUADPM="${QUADPM:-$PROJECT_ROOT/build/debug/cmd/quadpm/quadpm}"
EXTERNAL_MODULES_DIR="$TEMP_DIR/modules"
touch "$EXTERNAL_MODULES_PATHS_FILE"

if [ -f "$EXTERNAL_MODULES_FILE" ]; then
    echo "Installing external modules..."
    while IFS=' ' read -r module_name git_url || [ -n "$module_name" ]; do
        # Skip comments and empty lines
        [[ "$module_name" =~ ^# ]] && continue
        [[ -z "$module_name" ]] && continue

        # Check for pre-cloned source in sibling directory (CI environment)
        sibling_dir="$(dirname "$PROJECT_ROOT")/$module_name"
        if [ -f "$sibling_dir/module.qd" ]; then
            # Store parent directory as include path
            echo "$module_name $(dirname "$sibling_dir")" >> "$EXTERNAL_MODULES_PATHS_FILE"
            echo "  ✓ $module_name (pre-cloned)"
            continue
        fi

        # Check for local override via QUADRATE_EXTERNAL_MODULES env var
        if [ -n "${QUADRATE_EXTERNAL_MODULES:-}" ]; then
            local_dir="$QUADRATE_EXTERNAL_MODULES/$module_name"
            if [ -f "$local_dir/module.qd" ]; then
                # Store QUADRATE_EXTERNAL_MODULES as include path
                echo "$module_name $QUADRATE_EXTERNAL_MODULES" >> "$EXTERNAL_MODULES_PATHS_FILE"
                echo "  ✓ $module_name (local)"
                continue
            fi
        fi

        # Use quadpm to install module
        if QUADRATE_PATH="$EXTERNAL_MODULES_DIR" QUADRATE_LIBDIR="$PROJECT_ROOT/$QUADRATE_LIBDIR" \
           QDRT_INCLUDE="$PROJECT_ROOT/dist/include" \
           "$QUADPM" get "${git_url}@master" >/dev/null 2>&1; then
            # Use _namespaces directory as include path (symlinks provide clean names)
            ns_path="$EXTERNAL_MODULES_DIR/_namespaces/$module_name"
            if [ -L "$ns_path" ]; then
                echo "$module_name $EXTERNAL_MODULES_DIR/_namespaces" >> "$EXTERNAL_MODULES_PATHS_FILE"
                echo "  ✓ $module_name"
            else
                echo "  ✗ $module_name (installed but namespace not found)"
            fi
        else
            echo "  ✗ $module_name (install failed - tests will be skipped)"
        fi
    done < "$EXTERNAL_MODULES_FILE"
    echo ""
fi

# Helper function to get include flags for a test file
get_include_flags() {
    local test_file="$1"

    # Extract module name from path (e.g., tests/qd/hof/apply.qd -> hof)
    local rel_path="${test_file#$TEST_DIR_QD/}"
    local module_name="${rel_path%%/*}"

    # Look up include path from file (format: "module_name include_path")
    if [ -f "$EXTERNAL_MODULES_PATHS_FILE" ]; then
        local include_path=$(grep "^$module_name " "$EXTERNAL_MODULES_PATHS_FILE" 2>/dev/null | cut -d' ' -f2-)
        if [ -n "$include_path" ]; then
            echo "-I $include_path"
            return
        fi
    fi
    echo ""
}

# Helper function to check if a test should be skipped (external module not cloned)
should_skip_external() {
    local test_file="$1"

    # Extract module name from path
    local rel_path="${test_file#$TEST_DIR_QD/}"
    local module_name="${rel_path%%/*}"

    # Check if this module is in external_modules.txt but not in paths file (not cloned)
    if [ -f "$EXTERNAL_MODULES_FILE" ]; then
        if grep -q "^$module_name " "$EXTERNAL_MODULES_FILE" 2>/dev/null; then
            if ! grep -q "^$module_name " "$EXTERNAL_MODULES_PATHS_FILE" 2>/dev/null; then
                return 0  # Should skip
            fi
        fi
    fi
    return 1  # Should not skip
}
export -f get_include_flags should_skip_external
export EXTERNAL_MODULES_FILE EXTERNAL_MODULES_PATHS_FILE TEST_DIR_QD

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

    # Check if this test should be skipped (external module not cloned)
    if should_skip_external "$test_file"; then
        echo "SKIP:external module not available" >> "$result_file"
        echo -e "\033[1;33mSKIP\033[0m  $test_name (external module not available)"
        return
    fi

    # Get include flags for external modules
    local include_flags=$(get_include_flags "$test_file")

    # Check if this is a compile-time error test
    if [ -f "${test_file%.qd}.err" ]; then
        # Negative test - should fail to compile
        local expected_error_file="${test_file%.qd}.err"
        local actual_error_file="$TEMP_DIR/${test_id}.err"
        local binary="$TEMP_DIR/${test_id}"

        if "$compiler" $opt_flags $include_flags "$test_file" -o "$binary" 2>"$actual_error_file" >/dev/null; then
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

    # Check if this is a runtime error test
    if [ -f "${test_file%.qd}.runtime_err" ]; then
        # Runtime error test - should compile but fail at runtime
        local expected_error_file="${test_file%.qd}.runtime_err"
        local actual_error_file="$TEMP_DIR/${test_id}.runtime_err"
        local binary="$TEMP_DIR/${test_id}"

        # Compile should succeed
        if ! "$compiler" $opt_flags $include_flags "$test_file" -o "$binary" 2>/dev/null; then
            echo "FAIL:compilation failed (should have succeeded)" >> "$result_file"
            echo -e "\033[0;31mFAIL\033[0m  $test_name (compilation failed)"
            return
        fi

        # Run should fail
        if "$binary" >"$actual_error_file" 2>&1; then
            echo "FAIL:runtime succeeded (should have failed)" >> "$result_file"
            echo -e "\033[0;31mFAIL\033[0m  $test_name (runtime succeeded)"
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
            echo "FAIL:runtime error message mismatch" >> "$result_file"
            echo -e "\033[0;31mFAIL\033[0m  $test_name (runtime error message mismatch)"
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

    # Check if this is a test-mode file (has 'test "' but no 'fn main')
    local test_flag=""
    local is_test_mode="no"
    if grep -q 'test "' "$test_file" && ! grep -q 'fn main' "$test_file"; then
        test_flag="--test"
        is_test_mode="yes"
    fi

    # Compile (--test mode also runs the tests automatically)
    if [ "$is_test_mode" = "yes" ]; then
        # Test mode: compiler runs the tests, capture output
        # Use NO_COLOR to get clean output for comparison
        if ! NO_COLOR=1 "$compiler" $opt_flags $include_flags $test_flag "$test_file" -o "$binary" >"$actual_output_file" 2>"$compile_log"; then
            # Check if it's a compilation error or test failure
            if [ -s "$compile_log" ] && grep -q "error:" "$compile_log"; then
                echo "FAIL:compilation failed" >> "$result_file"
                echo -e "\033[0;31mFAIL\033[0m  $test_name (compilation failed)"
            else
                # Test failure - output is captured, continue to compare
                :
            fi
        fi
        # Skip to output comparison for test mode
    else
        # Normal mode: compile, then run
        if ! "$compiler" $opt_flags $include_flags "$test_file" -o "$binary" 2>"$compile_log" >/dev/null; then
            echo "FAIL:compilation failed" >> "$result_file"
            echo -e "\033[0;31mFAIL\033[0m  $test_name (compilation failed)"
            return
        fi
    fi

    # Run (with or without valgrind) - skip if test mode (already run)
    if [ "$is_test_mode" = "no" ]; then
        if [ "$use_valgrind" = "yes" ]; then
            local valgrind_log="$TEMP_DIR/${test_id}.valgrind"
            # Enhanced valgrind options:
            # --leak-check=full: Full leak detection
            # --show-leak-kinds=all: Show all leak types
            # --track-origins=yes: Track uninitialized values
            # --track-fds=yes: Detect file descriptor leaks
            # --error-exitcode=1: Exit with error code on issues
            if ! valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --track-fds=yes --error-exitcode=1 --log-file="$valgrind_log" "$binary" >"$actual_output_file" 2>&1; then
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
            # Use parallel if available (exclude network tests that require external services)
            find "$TEST_DIR_QD" -name "*.qd" -type f ! -path "*/network/*" | sort | \
                parallel --unsafe -j$(nproc) run_qd_test {} "$COMPILER" no ""
        else
            # Fallback to xargs for sequential execution (exclude network tests)
            find "$TEST_DIR_QD" -name "*.qd" -type f ! -path "*/network/*" | sort | \
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
        # Exclude network tests that require external services and are too slow under valgrind
        find "$TEST_DIR_QD" -name "*.qd" -type f ! -path "*/network/*" | sort | while read test_file; do
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
            # Use parallel if available (exclude network tests that require external services)
            find "$TEST_DIR_QD" -name "*.qd" -type f ! -path "*/network/*" | sort | \
                parallel --unsafe -j$(nproc) run_qd_test {} "$COMPILER" no "$OPT_LEVEL"
        else
            # Fallback to xargs for sequential execution (exclude network tests)
            find "$TEST_DIR_QD" -name "*.qd" -type f ! -path "*/network/*" | sort | \
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
        echo "  QUADC                      - Path to quadc compiler"
        echo "  QUADFMT                    - Path to quadfmt formatter"
        echo "  QUADUSES                   - Path to quaduses tool"
        echo "  QUADRATE_ROOT              - Path to standard library"
        echo "  QUADRATE_LIBDIR            - Path to libraries"
        echo "  QUADRATE_EXTERNAL_MODULES  - Path to local external modules (for development)"
        exit 1
        ;;
esac
