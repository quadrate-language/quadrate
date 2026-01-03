#!/bin/bash

# Unified test runner for Quadrate
# Usage:
#   ./tests/run_all.sh                    # Run all tests
#   ./tests/run_all.sh --failed           # Run only previously failed tests
#   ./tests/run_all.sh --test NAME        # Run specific test
#   ./tests/run_all.sh --suite SUITE      # Run specific suite (cpp, lsp, qd, formatter, linter, embed, quadpm)
#   ./tests/run_all.sh --clear            # Clear failed tests file
#   ./tests/run_all.sh --list             # List all available tests

set -u

# Get script and project directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Configuration
BUILD_DIR="${BUILD_DIR:-build/debug}"
QUADC="${QUADC:-$PROJECT_ROOT/$BUILD_DIR/cmd/quadc/quadc}"
QUADFMT="${QUADFMT:-$PROJECT_ROOT/dist/bin/quadfmt}"
QUADRATE_ROOT_DEFAULT="$PROJECT_ROOT/dist/share/quadrate"
QUADRATE_LIBDIR_DEFAULT="$PROJECT_ROOT/dist/lib"
FAILED_TESTS_FILE="$PROJECT_ROOT/.failed_tests"
TEMP_DIR="/tmp/quadrate_tests_$$"
PARALLEL_JOBS="${PARALLEL_JOBS:-$(nproc 2>/dev/null || echo 4)}"

# Note: Don't export QUADRATE_ROOT/LIBDIR globally - only for qd tests
# C++ unit tests expect these to not be set

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
DIM='\033[2m'
NC='\033[0m'

# Counters
declare -A SUITE_PASSED
declare -A SUITE_FAILED
declare -A SUITE_SKIPPED
TOTAL_PASSED=0
TOTAL_FAILED=0
TOTAL_SKIPPED=0

# Failed tests array
declare -a FAILED_TESTS_LIST=()

# Parse arguments
RUN_FAILED_ONLY=0
SPECIFIC_TEST=""
SPECIFIC_SUITE=""
LIST_TESTS=0
CLEAR_FAILED=0
VERBOSE=0
USE_VALGRIND=0

while [[ $# -gt 0 ]]; do
    case $1 in
        --failed|-f)
            RUN_FAILED_ONLY=1
            shift
            ;;
        --test|-t)
            SPECIFIC_TEST="$2"
            shift 2
            ;;
        --suite|-s)
            SPECIFIC_SUITE="$2"
            shift 2
            ;;
        --list|-l)
            LIST_TESTS=1
            shift
            ;;
        --clear|-c)
            CLEAR_FAILED=1
            shift
            ;;
        --verbose|-v)
            VERBOSE=1
            shift
            ;;
        --valgrind)
            USE_VALGRIND=1
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --failed, -f       Run only previously failed tests"
            echo "  --test, -t NAME    Run specific test by name"
            echo "  --suite, -s SUITE  Run specific suite (cpp, lsp, qd, formatter, linter, embed, quadpm, mtls)"
            echo "  --list, -l         List all available tests"
            echo "  --clear, -c        Clear failed tests file"
            echo "  --verbose, -v      Show verbose output"
            echo "  --valgrind         Run tests with valgrind (memory leak detection)"
            echo "  --help, -h         Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Clear failed tests file
if [[ $CLEAR_FAILED -eq 1 ]]; then
    rm -f "$FAILED_TESTS_FILE"
    echo "Cleared failed tests file"
    exit 0
fi

# Create temp directory
mkdir -p "$TEMP_DIR"
trap "rm -rf $TEMP_DIR" EXIT

# External module support
EXTERNAL_MODULES_FILE="$SCRIPT_DIR/external_modules.txt"
EXTERNAL_MODULES_PATHS_FILE="$TEMP_DIR/external_module_paths.txt"
QUADPM="${QUADPM:-$PROJECT_ROOT/$BUILD_DIR/cmd/quadpm/quadpm}"
EXTERNAL_MODULES_DIR="$TEMP_DIR/modules"
touch "$EXTERNAL_MODULES_PATHS_FILE"

# Install external modules (if external_modules.txt exists)
if [[ -f "$EXTERNAL_MODULES_FILE" ]]; then
    while IFS=' ' read -r module_name git_url || [[ -n "$module_name" ]]; do
        # Skip comments and empty lines
        [[ "$module_name" =~ ^# ]] && continue
        [[ -z "$module_name" ]] && continue

        # Check for pre-cloned source in sibling directory (CI environment)
        sibling_dir="$(dirname "$PROJECT_ROOT")/$module_name"
        if [[ -f "$sibling_dir/module.qd" ]]; then
            # Build native module if it has src/ directory
            if [[ -d "$sibling_dir/src" ]]; then
                (cd "$sibling_dir" && QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" "$QUADPM" build >/dev/null 2>&1) || true
            fi
            # Store parent directory as include path
            echo "$module_name $(dirname "$sibling_dir")" >> "$EXTERNAL_MODULES_PATHS_FILE"
            continue
        fi

        # Check for local override via QUADRATE_EXTERNAL_MODULES env var
        if [[ -n "${QUADRATE_EXTERNAL_MODULES:-}" ]]; then
            local_dir="$QUADRATE_EXTERNAL_MODULES/$module_name"
            if [[ -f "$local_dir/module.qd" ]]; then
                # Build native module if it has src/ directory
                if [[ -d "$local_dir/src" ]]; then
                    (cd "$local_dir" && QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" "$QUADPM" build >/dev/null 2>&1) || true
                fi
                echo "$module_name $QUADRATE_EXTERNAL_MODULES" >> "$EXTERNAL_MODULES_PATHS_FILE"
                continue
            fi
        fi

        # Use quadpm to install module
        if QUADRATE_PATH="$EXTERNAL_MODULES_DIR" QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" \
           QDRT_INCLUDE="$PROJECT_ROOT/dist/include" \
           "$QUADPM" get "${git_url}@master" >/dev/null 2>&1; then
            ns_path="$EXTERNAL_MODULES_DIR/_namespaces/$module_name"
            if [[ -L "$ns_path" ]]; then
                actual_dir=$(readlink -f "$ns_path")
                if [[ -d "$actual_dir/src" ]]; then
                    (cd "$actual_dir" && QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" "$QUADPM" build >/dev/null 2>&1) || true
                fi
                echo "$module_name $EXTERNAL_MODULES_DIR/_namespaces" >> "$EXTERNAL_MODULES_PATHS_FILE"
            fi
        fi
    done < "$EXTERNAL_MODULES_FILE"
fi

# Helper function to get include flags for a test file
get_include_flags() {
    local test_file="$1"
    local test_dir="$PROJECT_ROOT/tests/qd"

    # Extract module name from path (e.g., tests/qd/hof/apply.qd -> hof)
    local rel_path="${test_file#$test_dir/}"
    local module_name="${rel_path%%/*}"

    # Look up include path from file (format: "module_name include_path")
    if [[ -f "$EXTERNAL_MODULES_PATHS_FILE" ]]; then
        local include_path=$(grep "^$module_name " "$EXTERNAL_MODULES_PATHS_FILE" 2>/dev/null | cut -d' ' -f2-)
        if [[ -n "$include_path" ]]; then
            echo "-I $include_path"
            return
        fi
    fi
    echo ""
}

# Helper function to check if a test should be skipped (external module not available)
should_skip_external() {
    local test_file="$1"
    local test_dir="$PROJECT_ROOT/tests/qd"

    local rel_path="${test_file#$test_dir/}"
    local module_name="${rel_path%%/*}"

    # Check if this module is in external_modules.txt but not in paths file (not cloned)
    if [[ -f "$EXTERNAL_MODULES_FILE" ]]; then
        if grep -q "^$module_name " "$EXTERNAL_MODULES_FILE" 2>/dev/null; then
            if ! grep -q "^$module_name " "$EXTERNAL_MODULES_PATHS_FILE" 2>/dev/null; then
                return 0  # Should skip
            fi
        fi
    fi
    return 1  # Should not skip
}

# Progress tracking
CURRENT_TEST_NUM=0
CURRENT_TEST_TOTAL=0

# Logging functions
log_pass() {
    local suite="$1"
    local test_name="$2"
    local progress=""
    if [[ $CURRENT_TEST_TOTAL -gt 0 ]]; then
        progress="${DIM}[$CURRENT_TEST_NUM/$CURRENT_TEST_TOTAL]${NC} "
    fi
    echo -e "  ${GREEN}PASS${NC}  ${progress}$test_name"
    SUITE_PASSED[$suite]=$((${SUITE_PASSED[$suite]:-0} + 1))
    TOTAL_PASSED=$((TOTAL_PASSED + 1))
}

log_fail() {
    local suite="$1"
    local test_name="$2"
    local reason="${3:-}"
    local error_output="${4:-}"

    local progress=""
    if [[ $CURRENT_TEST_TOTAL -gt 0 ]]; then
        progress="${DIM}[$CURRENT_TEST_NUM/$CURRENT_TEST_TOTAL]${NC} "
    fi

    if [[ -n "$reason" ]]; then
        echo -e "  ${RED}FAIL${NC}  ${progress}$test_name ${DIM}($reason)${NC}"
    else
        echo -e "  ${RED}FAIL${NC}  ${progress}$test_name"
    fi

    # Show error output indented
    if [[ -n "$error_output" ]]; then
        echo "$error_output" | head -20 | sed 's/^/        /'
        local lines=$(echo "$error_output" | wc -l)
        if [[ $lines -gt 20 ]]; then
            echo -e "        ${DIM}... ($((lines - 20)) more lines)${NC}"
        fi
    fi

    SUITE_FAILED[$suite]=$((${SUITE_FAILED[$suite]:-0} + 1))
    TOTAL_FAILED=$((TOTAL_FAILED + 1))
    FAILED_TESTS_LIST+=("$suite:$test_name")
}

log_skip() {
    local suite="$1"
    local test_name="$2"
    local reason="${3:-}"

    local progress=""
    if [[ $CURRENT_TEST_TOTAL -gt 0 ]]; then
        progress="${DIM}[$CURRENT_TEST_NUM/$CURRENT_TEST_TOTAL]${NC} "
    fi

    if [[ -n "$reason" ]]; then
        echo -e "  ${YELLOW}SKIP${NC}  ${progress}$test_name ${DIM}($reason)${NC}"
    else
        echo -e "  ${YELLOW}SKIP${NC}  ${progress}$test_name"
    fi

    SUITE_SKIPPED[$suite]=$((${SUITE_SKIPPED[$suite]:-0} + 1))
    TOTAL_SKIPPED=$((TOTAL_SKIPPED + 1))
}

print_header() {
    echo ""
    echo -e "${BOLD}$1${NC}"
}

# Check if test should run (based on filters)
should_run_test() {
    local suite="$1"
    local test_name="$2"

    # Check suite filter
    if [[ -n "$SPECIFIC_SUITE" && "$suite" != "$SPECIFIC_SUITE" ]]; then
        return 1
    fi

    # Check specific test filter
    if [[ -n "$SPECIFIC_TEST" ]]; then
        if [[ "$test_name" != *"$SPECIFIC_TEST"* ]]; then
            return 1
        fi
    fi

    # Check failed-only filter
    if [[ $RUN_FAILED_ONLY -eq 1 ]]; then
        if [[ -f "$FAILED_TESTS_FILE" ]]; then
            if ! grep -q "^$suite:$test_name$" "$FAILED_TESTS_FILE" 2>/dev/null; then
                return 1
            fi
        else
            return 1
        fi
    fi

    return 0
}

# Run C++ tests via meson
run_cpp_tests() {
    local tests=("test_ast" "test_semantic_validator" "test_runtime" "test_llvmgen" "test_mem" "test_net")
    local suite="cpp"

    # Filter tests first
    local -a filtered_tests=()
    for test in "${tests[@]}"; do
        if should_run_test "$suite" "$test"; then
            filtered_tests+=("$test")
        fi
    done

    [[ ${#filtered_tests[@]} -eq 0 ]] && return

    if [[ $USE_VALGRIND -eq 1 ]]; then
        print_header "C++ Tests ${DIM}(valgrind)${NC}"
    else
        print_header "C++ Tests"
    fi

    CURRENT_TEST_TOTAL=${#filtered_tests[@]}
    CURRENT_TEST_NUM=0

    for test in "${filtered_tests[@]}"; do
        CURRENT_TEST_NUM=$((CURRENT_TEST_NUM + 1))

        local output
        local exit_code
        local valgrind_opt=""
        if [[ $USE_VALGRIND -eq 1 ]]; then
            valgrind_opt="--setup=valgrind"
        fi
        output=$(meson test -C "$PROJECT_ROOT/$BUILD_DIR" $valgrind_opt "$test" 2>&1)
        exit_code=$?

        if [[ $exit_code -eq 0 ]]; then
            log_pass "$suite" "$test"
        else
            local error_msg=$(echo "$output" | grep -A 50 "FAIL\|error\|Error\|valgrind" | head -20)
            log_fail "$suite" "$test" "test failed" "$error_msg"
        fi
    done

    CURRENT_TEST_TOTAL=0
}

# Run LSP tests via meson
run_lsp_tests() {
    # Skip stress test under valgrind (too slow)
    local tests
    if [[ $USE_VALGRIND -eq 1 ]]; then
        tests=("test_lsp" "test_lsp_extended")
    else
        tests=("test_lsp" "test_lsp_extended" "test_lsp_stress")
    fi
    local suite="lsp"

    # Filter tests first
    local -a filtered_tests=()
    for test in "${tests[@]}"; do
        if should_run_test "$suite" "$test"; then
            filtered_tests+=("$test")
        fi
    done

    [[ ${#filtered_tests[@]} -eq 0 ]] && return

    if [[ $USE_VALGRIND -eq 1 ]]; then
        print_header "LSP Tests ${DIM}(valgrind)${NC}"
    else
        print_header "LSP Tests"
    fi

    CURRENT_TEST_TOTAL=${#filtered_tests[@]}
    CURRENT_TEST_NUM=0

    for test in "${filtered_tests[@]}"; do
        CURRENT_TEST_NUM=$((CURRENT_TEST_NUM + 1))

        local output
        local exit_code
        local valgrind_opt=""
        if [[ $USE_VALGRIND -eq 1 ]]; then
            valgrind_opt="--setup=valgrind"
        fi
        output=$(meson test -C "$PROJECT_ROOT/$BUILD_DIR" $valgrind_opt "$test" 2>&1)
        exit_code=$?

        if [[ $exit_code -eq 0 ]]; then
            log_pass "$suite" "$test"
        else
            local error_msg=$(echo "$output" | grep -A 50 "FAIL\|error\|Error\|valgrind" | head -20)
            log_fail "$suite" "$test" "test failed" "$error_msg"
        fi
    done

    CURRENT_TEST_TOTAL=0
}

# Run a single QD test
run_single_qd_test() {
    local test_file="$1"
    local test_name="$2"
    local result_file="$TEMP_DIR/qd_${test_name//\//_}.result"

    local binary="$TEMP_DIR/qd_${test_name//\//_}"
    local actual_output="$TEMP_DIR/qd_${test_name//\//_}.out"
    local compile_log="$TEMP_DIR/qd_${test_name//\//_}.compile"

    # Check if this test should be skipped (external module not available)
    if should_skip_external "$test_file"; then
        echo "SKIP:external module not available" > "$result_file"
        return
    fi

    # Get include flags for external modules
    local include_flags=$(get_include_flags "$test_file")

    # Use relative path for test file (for error message matching)
    local rel_test_file="${test_file#$PROJECT_ROOT/}"

    # Check if this is a compile-time error test
    if [[ -f "${test_file%.qd}.err" ]]; then
        local expected_error_file="${test_file%.qd}.err"

        # Run from project root with relative path
        if (cd "$PROJECT_ROOT" && "$QUADC" $include_flags "$rel_test_file" -o "$binary" 2>"$compile_log" >/dev/null); then
            echo "FAIL:compilation succeeded (should have failed)" > "$result_file"
            return
        fi

        # Check if all error patterns are present
        local all_patterns_found=true
        while IFS= read -r pattern; do
            if ! grep -qF "$pattern" "$compile_log"; then
                all_patterns_found=false
                break
            fi
        done < "$expected_error_file"

        if $all_patterns_found; then
            echo "PASS" > "$result_file"
        else
            echo "FAIL:error message mismatch" > "$result_file"
            echo "Expected patterns from $expected_error_file:" >> "$result_file"
            cat "$expected_error_file" >> "$result_file"
            echo "---" >> "$result_file"
            echo "Actual output:" >> "$result_file"
            cat "$compile_log" >> "$result_file"
        fi
        return
    fi

    # Check if this is a runtime error test
    if [[ -f "${test_file%.qd}.runtime_err" ]]; then
        local expected_error_file="${test_file%.qd}.runtime_err"

        # Compile should succeed (use relative path)
        if ! (cd "$PROJECT_ROOT" && "$QUADC" $include_flags "$rel_test_file" -o "$binary" 2>"$compile_log" >/dev/null); then
            echo "FAIL:compilation failed (should have succeeded)" > "$result_file"
            cat "$compile_log" >> "$result_file"
            return
        fi

        # Run should fail
        if "$binary" >"$actual_output" 2>&1; then
            echo "FAIL:runtime succeeded (should have failed)" > "$result_file"
            return
        fi

        # Check if all error patterns are present
        local all_patterns_found=true
        while IFS= read -r pattern; do
            if ! grep -qF "$pattern" "$actual_output"; then
                all_patterns_found=false
                break
            fi
        done < "$expected_error_file"

        if $all_patterns_found; then
            echo "PASS" > "$result_file"
        else
            echo "FAIL:runtime error message mismatch" > "$result_file"
            echo "Expected patterns:" >> "$result_file"
            cat "$expected_error_file" >> "$result_file"
            echo "---" >> "$result_file"
            echo "Actual output:" >> "$result_file"
            cat "$actual_output" >> "$result_file"
        fi
        return
    fi

    # Check for expected output file (.expected or .out)
    local expected_file="${test_file%.qd}.expected"
    if [[ ! -f "$expected_file" ]]; then
        expected_file="${test_file%.qd}.out"
        if [[ ! -f "$expected_file" ]]; then
            echo "SKIP:no expected output" > "$result_file"
            return
        fi
    fi

    # Check if this is a test-mode file
    local test_flag=""
    if grep -q 'test "' "$test_file" && ! grep -q 'fn main' "$test_file"; then
        test_flag="--test"
    fi

    # Compile (use relative path for consistent error messages)
    if [[ -n "$test_flag" ]]; then
        if ! (cd "$PROJECT_ROOT" && NO_COLOR=1 "$QUADC" $include_flags $test_flag "$rel_test_file" -o "$binary" >"$actual_output" 2>"$compile_log"); then
            if [[ -s "$compile_log" ]] && grep -q "error:" "$compile_log"; then
                echo "FAIL:compilation failed" > "$result_file"
                cat "$compile_log" >> "$result_file"
                return
            fi
        fi
    else
        if ! (cd "$PROJECT_ROOT" && "$QUADC" $include_flags "$rel_test_file" -o "$binary" 2>"$compile_log" >/dev/null); then
            echo "FAIL:compilation failed" > "$result_file"
            cat "$compile_log" >> "$result_file"
            return
        fi

        # Run (with or without valgrind)
        if [[ "${USE_VALGRIND:-0}" -eq 1 ]]; then
            local valgrind_log="$TEMP_DIR/qd_${test_name//\//_}.valgrind"
            if ! valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 --log-file="$valgrind_log" "$binary" >"$actual_output" 2>&1; then
                # Check if it's a valgrind error or runtime error
                if grep -q "ERROR SUMMARY: [1-9]" "$valgrind_log" 2>/dev/null; then
                    echo "FAIL:valgrind errors" > "$result_file"
                    grep -A 20 "ERROR SUMMARY\|definitely lost\|Invalid" "$valgrind_log" >> "$result_file" 2>/dev/null
                else
                    echo "FAIL:runtime error" > "$result_file"
                    cat "$actual_output" >> "$result_file"
                fi
                return
            fi
        else
            if ! "$binary" >"$actual_output" 2>&1; then
                echo "FAIL:runtime error" > "$result_file"
                cat "$actual_output" >> "$result_file"
                return
            fi
        fi
    fi

    # Compare output
    if diff -q "$expected_file" "$actual_output" >/dev/null 2>&1; then
        echo "PASS" > "$result_file"
    else
        echo "FAIL:output mismatch" > "$result_file"
        diff -u "$expected_file" "$actual_output" >> "$result_file" 2>&1 || true
    fi
}

# Run QD language tests
run_qd_tests() {
    local suite="qd"
    local test_dir="$PROJECT_ROOT/tests/qd"

    # Find all test files
    local -a test_files=()
    while IFS= read -r -d '' file; do
        test_files+=("$file")
    done < <(find "$test_dir" -name "*.qd" -type f ! -path "*/network/*" -print0 | sort -z)

    # Filter tests
    local -a filtered_tests=()
    for test_file in "${test_files[@]}"; do
        local rel_path="${test_file#$test_dir/}"
        local test_name="${rel_path%.qd}"
        if should_run_test "$suite" "$test_name"; then
            filtered_tests+=("$test_file")
        fi
    done

    [[ ${#filtered_tests[@]} -eq 0 ]] && return

    local total=${#filtered_tests[@]}
    if [[ $USE_VALGRIND -eq 1 ]]; then
        print_header "Language Tests ${DIM}[$total tests, valgrind]${NC}"
    else
        print_header "Language Tests ${DIM}[$total tests]${NC}"
    fi

    # Export for parallel execution (set env vars for qd tests)
    export QUADC TEMP_DIR PROJECT_ROOT USE_VALGRIND
    export QUADRATE_ROOT="${QUADRATE_ROOT:-$QUADRATE_ROOT_DEFAULT}"
    export QUADRATE_LIBDIR="${QUADRATE_LIBDIR:-$QUADRATE_LIBDIR_DEFAULT}"
    export -f run_single_qd_test

    CURRENT_TEST_TOTAL=$total
    CURRENT_TEST_NUM=0

    # Run tests - sequentially under valgrind, batched otherwise
    if [[ $USE_VALGRIND -eq 1 ]]; then
        # Sequential execution for valgrind
        for test_file in "${filtered_tests[@]}"; do
            local rel_path="${test_file#$test_dir/}"
            local test_name="${rel_path%.qd}"

            CURRENT_TEST_NUM=$((CURRENT_TEST_NUM + 1))
            run_single_qd_test "$test_file" "$test_name" 2>/dev/null

            # Print result immediately
            local result_file="$TEMP_DIR/qd_${test_name//\//_}.result"
            if [[ ! -f "$result_file" ]]; then
                log_skip "$suite" "$test_name" "no result"
                continue
            fi

            local status=$(head -1 "$result_file")
            local error_output=$(tail -n +2 "$result_file")

            case "$status" in
                PASS)
                    log_pass "$suite" "$test_name"
                    ;;
                SKIP*)
                    local reason="${status#SKIP:}"
                    log_skip "$suite" "$test_name" "$reason"
                    ;;
                FAIL*)
                    local reason="${status#FAIL:}"
                    log_fail "$suite" "$test_name" "$reason" "$error_output"
                    ;;
                *)
                    log_fail "$suite" "$test_name" "unknown status" "$status"
                    ;;
            esac
        done
    else
        # Batched parallel execution
        local batch_size=$PARALLEL_JOBS
        local i=0
        local batch_start=0

        while [[ $batch_start -lt ${#filtered_tests[@]} ]]; do
            # Start a batch of tests
            local batch_end=$((batch_start + batch_size))
            if [[ $batch_end -gt ${#filtered_tests[@]} ]]; then
                batch_end=${#filtered_tests[@]}
            fi

            local batch_tests=()
            for ((i=batch_start; i<batch_end; i++)); do
                local test_file="${filtered_tests[$i]}"
                local rel_path="${test_file#$test_dir/}"
                local test_name="${rel_path%.qd}"
                batch_tests+=("$test_name")

                # Run in background
                run_single_qd_test "$test_file" "$test_name" 2>/dev/null &
            done

            # Wait for batch to complete
            wait 2>/dev/null

            # Print results for this batch
            for test_name in "${batch_tests[@]}"; do
                CURRENT_TEST_NUM=$((CURRENT_TEST_NUM + 1))
                local result_file="$TEMP_DIR/qd_${test_name//\//_}.result"

                if [[ ! -f "$result_file" ]]; then
                    log_skip "$suite" "$test_name" "no result"
                    continue
                fi

                local status=$(head -1 "$result_file")
                local error_output=$(tail -n +2 "$result_file")

                case "$status" in
                    PASS)
                        log_pass "$suite" "$test_name"
                        ;;
                    SKIP*)
                        local reason="${status#SKIP:}"
                        log_skip "$suite" "$test_name" "$reason"
                        ;;
                    FAIL*)
                        local reason="${status#FAIL:}"
                        log_fail "$suite" "$test_name" "$reason" "$error_output"
                        ;;
                    *)
                        log_fail "$suite" "$test_name" "unknown status" "$status"
                        ;;
                esac
            done

            batch_start=$batch_end
        done
    fi

    CURRENT_TEST_TOTAL=0
}

# Run formatter tests
run_formatter_tests() {
    local suite="formatter"
    local test_dir="$PROJECT_ROOT/tests/formatter"
    local expected_dir="$test_dir/expected"

    # Find all test files
    local -a test_files=()
    while IFS= read -r -d '' file; do
        test_files+=("$file")
    done < <(find "$test_dir" -maxdepth 1 -name "*.qd" -type f -print0 | sort -z)

    # Filter tests
    local -a filtered_tests=()
    for test_file in "${test_files[@]}"; do
        local test_name=$(basename "$test_file" .qd)
        if should_run_test "$suite" "$test_name"; then
            filtered_tests+=("$test_file")
        fi
    done

    [[ ${#filtered_tests[@]} -eq 0 ]] && return

    print_header "Formatter Tests"

    CURRENT_TEST_TOTAL=${#filtered_tests[@]}
    CURRENT_TEST_NUM=0

    for test_file in "${filtered_tests[@]}"; do
        CURRENT_TEST_NUM=$((CURRENT_TEST_NUM + 1))
        local test_name=$(basename "$test_file" .qd)
        local expected_file="$expected_dir/${test_name}.qd"
        local output_file="$TEMP_DIR/fmt_${test_name}.qd"

        if [[ ! -f "$expected_file" ]]; then
            log_skip "$suite" "$test_name" "no expected output"
            continue
        fi

        cp "$test_file" "$output_file"
        local fmt_output
        if ! fmt_output=$("$QUADFMT" -w "$output_file" 2>&1); then
            log_fail "$suite" "$test_name" "formatter failed" "$fmt_output"
            continue
        fi

        if diff -q "$expected_file" "$output_file" >/dev/null 2>&1; then
            log_pass "$suite" "$test_name"
        else
            local diff_output=$(diff -u "$expected_file" "$output_file" 2>&1)
            log_fail "$suite" "$test_name" "output mismatch" "$diff_output"
        fi
    done

    CURRENT_TEST_TOTAL=0
}

# Run linter tests via meson
run_linter_tests() {
    local suite="linter"

    if ! should_run_test "$suite" "linter_tests"; then
        return
    fi

    if [[ $USE_VALGRIND -eq 1 ]]; then
        print_header "Linter Tests ${DIM}(valgrind)${NC}"
    else
        print_header "Linter Tests"
    fi

    local output
    local exit_code
    local valgrind_opt=""
    if [[ $USE_VALGRIND -eq 1 ]]; then
        valgrind_opt="--setup=valgrind"
    fi
    output=$(meson test -C "$PROJECT_ROOT/$BUILD_DIR" $valgrind_opt --suite linter 2>&1)
    exit_code=$?

    if [[ $exit_code -eq 0 ]]; then
        log_pass "$suite" "linter_tests"
    else
        local error_msg=$(echo "$output" | grep -A 50 "FAIL\|error\|Error\|valgrind" | head -20)
        log_fail "$suite" "linter_tests" "test failed" "$error_msg"
    fi
}

# Run embed tests
run_embed_tests() {
    local suite="embed"
    local -a tests=(
        "embed:Hello, World!"
        "multi-module-test:Math module:"
        "native-functions-test:Current timestamp:"
        "incremental-test:Building all at once"
        "ffi:Hello, World!"
    )

    # Filter tests
    local -a filtered_tests=()
    for test_spec in "${tests[@]}"; do
        local test_name="${test_spec%%:*}"
        if should_run_test "$suite" "$test_name"; then
            filtered_tests+=("$test_spec")
        fi
    done

    [[ ${#filtered_tests[@]} -eq 0 ]] && return

    if [[ $USE_VALGRIND -eq 1 ]]; then
        print_header "Embed Tests ${DIM}(valgrind)${NC}"
    else
        print_header "Embed Tests"
    fi

    CURRENT_TEST_TOTAL=${#filtered_tests[@]}
    CURRENT_TEST_NUM=0

    for test_spec in "${filtered_tests[@]}"; do
        CURRENT_TEST_NUM=$((CURRENT_TEST_NUM + 1))
        local test_name="${test_spec%%:*}"
        local expected_pattern="${test_spec#*:}"
        local test_exe="$PROJECT_ROOT/dist/examples/$test_name"

        if [[ ! -x "$test_exe" ]]; then
            log_skip "$suite" "$test_name" "executable not found"
            continue
        fi

        local output
        local exit_code

        if [[ $USE_VALGRIND -eq 1 ]]; then
            local valgrind_log="$TEMP_DIR/embed_${test_name}.valgrind"
            output=$(LD_LIBRARY_PATH="$PROJECT_ROOT/dist/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=99 --log-file="$valgrind_log" "$test_exe" 2>&1)
            exit_code=$?

            if [[ $exit_code -eq 99 ]]; then
                # Valgrind detected errors
                local valgrind_errors=$(grep -A 20 "ERROR SUMMARY\|definitely lost\|Invalid" "$valgrind_log" 2>/dev/null | head -20)
                log_fail "$suite" "$test_name" "valgrind errors" "$valgrind_errors"
                continue
            fi
        else
            output=$(LD_LIBRARY_PATH="$PROJECT_ROOT/dist/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$test_exe" 2>&1)
            exit_code=$?
        fi

        if [[ $exit_code -eq 0 ]] && echo "$output" | grep -q "$expected_pattern"; then
            log_pass "$suite" "$test_name"
        else
            log_fail "$suite" "$test_name" "unexpected output" "$output"
        fi
    done

    CURRENT_TEST_TOTAL=0
}

# Run quadpm tests via meson
run_quadpm_tests() {
    local suite="quadpm"

    if ! should_run_test "$suite" "quadpm_tests"; then
        return
    fi

    print_header "Package Manager Tests"

    local output
    local exit_code
    output=$(meson test -C "$PROJECT_ROOT/$BUILD_DIR" --suite quadpm 2>&1)
    exit_code=$?

    if [[ $exit_code -eq 0 ]]; then
        log_pass "$suite" "quadpm_tests"
    else
        local error_msg=$(echo "$output" | grep -A 50 "FAIL\|error\|Error" | head -20)
        log_fail "$suite" "quadpm_tests" "test failed" "$error_msg"
    fi
}

# Run mTLS tests
run_mtls_tests() {
    local suite="mtls"

    if ! should_run_test "$suite" "mtls_test"; then
        return
    fi

    # Check for openssl
    if ! command -v openssl &> /dev/null; then
        log_skip "$suite" "mtls_test" "openssl not found"
        return
    fi

    if [[ $USE_VALGRIND -eq 1 ]]; then
        print_header "mTLS Tests ${DIM}(valgrind)${NC}"
    else
        print_header "mTLS Tests"
    fi

    local output
    local exit_code
    local valgrind_arg=""
    if [[ $USE_VALGRIND -eq 1 ]]; then
        valgrind_arg="valgrind"
    fi

    output=$(QUADC="$QUADC" QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" QUADRATE_EXTERNAL_MODULES="${QUADRATE_EXTERNAL_MODULES:-}" bash "$PROJECT_ROOT/tests/run_mtls_test.sh" $valgrind_arg 2>&1)
    exit_code=$?

    if [[ $exit_code -eq 0 ]]; then
        log_pass "$suite" "mtls_test"
    else
        local error_msg=$(echo "$output" | grep -A 20 "FAIL\|error\|Error" | head -20)
        log_fail "$suite" "mtls_test" "test failed" "$error_msg"
    fi
}

# List all available tests
list_all_tests() {
    echo "Available tests:"
    echo ""

    echo "C++ Tests (suite: cpp):"
    for test in test_ast test_semantic_validator test_runtime test_llvmgen test_mem test_net; do
        echo "  $test"
    done
    echo ""

    echo "LSP Tests (suite: lsp):"
    for test in test_lsp test_lsp_extended test_lsp_stress; do
        echo "  $test"
    done
    echo ""

    echo "Language Tests (suite: qd):"
    find "$PROJECT_ROOT/tests/qd" -name "*.qd" -type f ! -path "*/network/*" | sort | while read -r file; do
        local rel="${file#$PROJECT_ROOT/tests/qd/}"
        echo "  ${rel%.qd}"
    done
    echo ""

    echo "Formatter Tests (suite: formatter):"
    find "$PROJECT_ROOT/tests/formatter" -maxdepth 1 -name "*.qd" -type f | sort | while read -r file; do
        echo "  $(basename "$file" .qd)"
    done
    echo ""

    echo "Linter Tests (suite: linter):"
    echo "  linter_tests"
    echo ""

    echo "Embed Tests (suite: embed):"
    for test in embed multi-module-test native-functions-test incremental-test ffi; do
        echo "  $test"
    done
    echo ""

    echo "Package Manager Tests (suite: quadpm):"
    echo "  quadpm_tests"
    echo ""

    echo "mTLS Tests (suite: mtls):"
    echo "  mtls_test"
}

# Print summary
print_summary() {
    echo ""
    echo -e "${BOLD}═══════════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLD}  Summary${NC}"
    echo -e "${BOLD}═══════════════════════════════════════════════════════════════════════════════${NC}"

    # Print per-suite summary
    for suite in cpp lsp qd formatter linter embed quadpm mtls; do
        local passed=${SUITE_PASSED[$suite]:-0}
        local failed=${SUITE_FAILED[$suite]:-0}
        local skipped=${SUITE_SKIPPED[$suite]:-0}
        local total=$((passed + failed + skipped))

        [[ $total -eq 0 ]] && continue

        local suite_name
        case $suite in
            cpp) suite_name="C++ Tests" ;;
            lsp) suite_name="LSP Tests" ;;
            qd) suite_name="Language Tests" ;;
            formatter) suite_name="Formatter" ;;
            linter) suite_name="Linter" ;;
            embed) suite_name="Embed" ;;
            quadpm) suite_name="Package Manager" ;;
            mtls) suite_name="mTLS" ;;
        esac

        printf "  %-20s" "$suite_name:"
        echo -ne "${GREEN}$passed passed${NC}"
        if [[ $failed -gt 0 ]]; then
            echo -ne ", ${RED}$failed failed${NC}"
        fi
        if [[ $skipped -gt 0 ]]; then
            echo -ne ", ${YELLOW}$skipped skipped${NC}"
        fi
        echo ""
    done

    echo -e "───────────────────────────────────────────────────────────────────────────────"

    local total=$((TOTAL_PASSED + TOTAL_FAILED + TOTAL_SKIPPED))
    echo -ne "  Total: ${GREEN}$TOTAL_PASSED passed${NC}"
    if [[ $TOTAL_FAILED -gt 0 ]]; then
        echo -ne ", ${RED}$TOTAL_FAILED failed${NC}"
    fi
    if [[ $TOTAL_SKIPPED -gt 0 ]]; then
        echo -ne ", ${YELLOW}$TOTAL_SKIPPED skipped${NC}"
    fi
    echo ""
    echo ""

    # Write failed tests to file
    if [[ $TOTAL_FAILED -gt 0 ]]; then
        echo "# Failed tests from $(date '+%Y-%m-%d %H:%M:%S')" > "$FAILED_TESTS_FILE"
        for test in "${FAILED_TESTS_LIST[@]}"; do
            echo "$test" >> "$FAILED_TESTS_FILE"
        done
        echo -e "${YELLOW}$TOTAL_FAILED failing test(s) written to .failed_tests${NC}"
        echo -e "Re-run with: ${CYAN}make tests-failed${NC}"
        echo ""
        echo -e "${RED}FAILED${NC}"
        exit 1
    else
        # Clear failed tests file on success
        if [[ $RUN_FAILED_ONLY -eq 1 ]]; then
            rm -f "$FAILED_TESTS_FILE"
            echo -e "${GREEN}All previously failing tests now pass!${NC}"
            echo -e "Run full suite with: ${CYAN}make tests${NC}"
        else
            rm -f "$FAILED_TESTS_FILE"
        fi
        echo ""
        echo -e "${GREEN}PASSED${NC}"
        exit 0
    fi
}

# Main execution
main() {
    if [[ $LIST_TESTS -eq 1 ]]; then
        list_all_tests
        exit 0
    fi

    echo -e "${BOLD}═══════════════════════════════════════════════════════════════════════════════${NC}"
    local mode_info=""
    if [[ $USE_VALGRIND -eq 1 ]]; then
        mode_info="${CYAN}valgrind${NC}"
    fi
    if [[ $RUN_FAILED_ONLY -eq 1 ]]; then
        local failed_count=0
        if [[ -f "$FAILED_TESTS_FILE" ]]; then
            failed_count=$(grep -v '^#' "$FAILED_TESTS_FILE" | wc -l)
        fi
        echo -e "${BOLD}  Quadrate Test Suite ${DIM}($failed_count previously failed${mode_info:+, }${mode_info})${NC}"
    elif [[ -n "$SPECIFIC_TEST" ]]; then
        echo -e "${BOLD}  Quadrate Test Suite ${DIM}(test: $SPECIFIC_TEST${mode_info:+, }${mode_info})${NC}"
    elif [[ -n "$SPECIFIC_SUITE" ]]; then
        echo -e "${BOLD}  Quadrate Test Suite ${DIM}(suite: $SPECIFIC_SUITE${mode_info:+, }${mode_info})${NC}"
    elif [[ -n "$mode_info" ]]; then
        echo -e "${BOLD}  Quadrate Test Suite ${DIM}(${mode_info})${NC}"
    else
        echo -e "${BOLD}  Quadrate Test Suite${NC}"
    fi
    echo -e "${BOLD}═══════════════════════════════════════════════════════════════════════════════${NC}"

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "cpp" ]]; then
        run_cpp_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "lsp" ]]; then
        run_lsp_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "qd" ]]; then
        run_qd_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "formatter" ]]; then
        run_formatter_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "linter" ]]; then
        run_linter_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "embed" ]]; then
        run_embed_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "quadpm" ]]; then
        run_quadpm_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "mtls" ]]; then
        run_mtls_tests
    fi

    print_summary
}

main
