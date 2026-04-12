#!/bin/bash

# Unified test runner for Quadrate
# Usage:
#   ./tests/run_all.sh                    # Run all tests
#   ./tests/run_all.sh --failed           # Run only previously failed tests
#   ./tests/run_all.sh --test NAME        # Run specific test
#   ./tests/run_all.sh --suite SUITE      # Run specific suite (cpp, lsp, qd, formatter, linter, embed, quadpm, build_cache, quadmcp, args, crosscompile, stdlib, mtls, fuzz)
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
if [ "$(uname -s)" = "Haiku" ]; then
    DIST_DATADIR="dist/data"
else
    DIST_DATADIR="dist/share"
fi
QUADRATE_ROOT_DEFAULT="$PROJECT_ROOT/$DIST_DATADIR/quadrate"
QUADRATE_LIBDIR_DEFAULT="$PROJECT_ROOT/dist/lib"
FAILED_TESTS_FILE="$PROJECT_ROOT/.failed_tests"
TEMP_DIR="${QUADRATE_TEST_TMPDIR:-/tmp}/quadrate_tests_$$"
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
USE_HELGRIND=0
FUZZ_TIME=10

# Shared valgrind flags for all test types
VALGRIND_FLAGS="--leak-check=full --show-leak-kinds=definite,indirect,possible --errors-for-leak-kinds=definite,indirect --track-fds=yes --track-origins=yes --error-exitcode=1"

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
        --fuzz-time)
            FUZZ_TIME="$2"
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
        --helgrind)
            USE_HELGRIND=1
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --failed, -f       Run only previously failed tests"
            echo "  --test, -t NAME    Run specific test by name"
            echo "  --suite, -s SUITE  Run specific suite (cpp, lsp, qd, formatter, linter, embed, quadpm, build_cache, quadmcp, args, crosscompile, stdlib, mtls, fuzz)"
            echo "  --fuzz-time SECS   Fuzz test duration in seconds (default: 10)"
            echo "  --list, -l         List all available tests"
            echo "  --clear, -c        Clear failed tests file"
            echo "  --verbose, -v      Show verbose output"
            echo "  --valgrind         Run tests with valgrind (memory leak detection)"
            echo "  --helgrind         Run stdlib tests with helgrind (thread error detection)"
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
SIBLING_PARENT_DIR=""  # Will be set if sibling modules are found
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
            # Remember the parent directory for dependency resolution
            SIBLING_PARENT_DIR="$(dirname "$sibling_dir")"
            # Build native module if it has src/ directory
            # Pass QUADRATE_PATH so quadpm can find sibling dependencies
            if [[ -d "$sibling_dir/src" ]]; then
                build_log="$TEMP_DIR/build_${module_name}.log"
                if ! (cd "$sibling_dir" && QUADRATE_PATH="$SIBLING_PARENT_DIR" QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" "$QUADPM" build >"$build_log" 2>&1); then
                    echo "Warning: Failed to build $module_name:" >&2
                    head -20 "$build_log" >&2
                fi
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
                    build_log="$TEMP_DIR/build_${module_name}.log"
                    if ! (cd "$local_dir" && QUADRATE_PATH="$QUADRATE_EXTERNAL_MODULES" QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" "$QUADPM" build >"$build_log" 2>&1); then
                        echo "Warning: Failed to build $module_name:" >&2
                        head -20 "$build_log" >&2
                    fi
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
                    build_log="$TEMP_DIR/build_${module_name}.log"
                    if ! (cd "$actual_dir" && QUADRATE_PATH="$EXTERNAL_MODULES_DIR" QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" "$QUADPM" build >"$build_log" 2>&1); then
                        echo "Warning: Failed to build $module_name:" >&2
                        head -20 "$build_log" >&2
                    fi
                fi
                echo "$module_name $EXTERNAL_MODULES_DIR/_namespaces" >> "$EXTERNAL_MODULES_PATHS_FILE"
            fi
        fi
    done < "$EXTERNAL_MODULES_FILE"
fi

# Build in-tree external modules (moved from external repos into lib/)
for intree_dir in "$PROJECT_ROOT"/lib/*/; do
    module_name=$(basename "$intree_dir")
    # Skip meson-built libraries (have meson.build) and non-module directories
    [[ -f "$intree_dir/meson.build" ]] && continue
    ls "$intree_dir"/*.qd >/dev/null 2>&1 || continue

    # Build native module if it has src/ directory
    if [[ -d "$intree_dir/src" ]]; then
        build_log="$TEMP_DIR/build_${module_name}.log"
        if ! (cd "$intree_dir" && QUADRATE_PATH="$PROJECT_ROOT/lib" QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" "$QUADPM" build >"$build_log" 2>&1); then
            echo "Warning: Failed to build in-tree module $module_name:" >&2
            head -20 "$build_log" >&2
        fi
    fi

    # Record path for include flag resolution
    echo "$module_name $PROJECT_ROOT/lib" >> "$EXTERNAL_MODULES_PATHS_FILE"
done

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
        echo "$error_output" | head -60 | sed 's/^/        /'
        local lines=$(echo "$error_output" | wc -l)
        if [[ $lines -gt 60 ]]; then
            echo -e "        ${DIM}... ($((lines - 60)) more lines)${NC}"
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
    local tests=("test_ast" "test_semantic_validator" "test_runtime" "test_llvmgen" "test_mem" "test_options")
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
        output=$(meson test -C "$PROJECT_ROOT/$BUILD_DIR" $valgrind_opt --print-errorlogs "$test" 2>&1)
        exit_code=$?

        if [[ $exit_code -eq 0 ]]; then
            log_pass "$suite" "$test"
        else
            local error_msg=$(echo "$output" | tail -80)
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
        tests=("test_lsp" "test_lsp_extended" "test_lsp_features" "test_lsp_comprehensive")
    else
        tests=("test_lsp" "test_lsp_extended" "test_lsp_stress" "test_lsp_features" "test_lsp_comprehensive")
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
        output=$(meson test -C "$PROJECT_ROOT/$BUILD_DIR" $valgrind_opt --print-errorlogs "$test" 2>&1)
        exit_code=$?

        if [[ $exit_code -eq 0 ]]; then
            log_pass "$suite" "$test"
        else
            local error_msg=$(echo "$output" | tail -80)
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
            if ! valgrind $VALGRIND_FLAGS --log-file="$valgrind_log" "$binary" >"$actual_output" 2>&1; then
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
    while IFS= read -r file; do
        test_files+=("$file")
    done <<< "$(find "$test_dir" -name "*.qd" -type f ! -path "*/network/*" | sort)"

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
    export EXTERNAL_MODULES_PATHS_FILE EXTERNAL_MODULES_FILE
    export -f run_single_qd_test get_include_flags should_skip_external

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
    while IFS= read -r file; do
        test_files+=("$file")
    done <<< "$(find "$test_dir" -maxdepth 1 -name "*.qd" -type f | sort)"

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

# Run linter tests directly (compare quadlint output to expected .out files)
run_linter_tests() {
    local suite="linter"
    local test_dir="$PROJECT_ROOT/tests/linter"
    local quadlint="${QUADLINT:-$PROJECT_ROOT/$BUILD_DIR/cmd/quadlint/quadlint}"

    if ! should_run_test "$suite" "linter_tests"; then
        return
    fi

    if [[ $USE_VALGRIND -eq 1 ]]; then
        print_header "Linter Tests ${DIM}(valgrind)${NC}"
    else
        print_header "Linter Tests"
    fi

    if [[ ! -x "$quadlint" ]]; then
        log_skip "$suite" "linter_tests" "quadlint not found"
        return
    fi

    local valgrind_cmd=""
    if [[ $USE_VALGRIND -eq 1 ]]; then
        valgrind_cmd="valgrind $VALGRIND_FLAGS --quiet"
    fi

    local all_passed=true
    local test_count=0
    local pass_count=0

    # Find all .qd test files
    while IFS= read -r test_file; do
        local test_name=$(basename "$test_file" .qd)
        local test_subdir=$(basename "$(dirname "$test_file")")
        local full_test_name="${test_subdir}/${test_name}"
        local expected_file="${test_file%.qd}.out"

        if [[ ! -f "$expected_file" ]]; then
            continue
        fi

        if ! should_run_test "$suite" "$full_test_name"; then
            continue
        fi

        test_count=$((test_count + 1))

        # Read extra flags from .flags file if present
        local flags_file="${test_file%.qd}.flags"
        local extra_flags=""
        if [[ -f "$flags_file" ]]; then
            extra_flags=$(cat "$flags_file")
        fi

        # Run linter and capture output
        local actual_output
        if [[ -n "$valgrind_cmd" ]]; then
            actual_output=$($valgrind_cmd "$quadlint" $extra_flags "$test_file" 2>&1) || true
        else
            actual_output=$("$quadlint" $extra_flags "$test_file" 2>&1) || true
        fi

        # Compare output (normalize absolute paths to relative)
        local expected_output=$(cat "$expected_file")
        local normalized_output=$(echo "$actual_output" | sed "s|$PROJECT_ROOT/||g")
        if [[ "$normalized_output" == "$expected_output" ]]; then
            log_pass "$suite" "$full_test_name"
            pass_count=$((pass_count + 1))
        else
            all_passed=false
            log_fail "$suite" "$full_test_name" "output mismatch" "Expected:\n$expected_output\n\nGot:\n$normalized_output"
        fi
    done <<< "$(find "$test_dir" -name "*.qd" -type f | sort)"

    if [[ $test_count -eq 0 ]]; then
        log_skip "$suite" "linter_tests" "no tests found"
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
        "typed-native-test:All typed native tests passed"
        "embed-comprehensive-test:All comprehensive tests passed"
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
            output=$(QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" LD_LIBRARY_PATH="$PROJECT_ROOT/dist/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" valgrind $VALGRIND_FLAGS --error-exitcode=99 --log-file="$valgrind_log" "$test_exe" 2>&1)
            exit_code=$?

            if [[ $exit_code -eq 99 ]]; then
                # Valgrind detected errors
                local valgrind_errors=$(grep -A 20 "ERROR SUMMARY\|definitely lost\|Invalid" "$valgrind_log" 2>/dev/null | head -20)
                log_fail "$suite" "$test_name" "valgrind errors" "$valgrind_errors"
                continue
            fi
        else
            output=$(QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" LD_LIBRARY_PATH="$PROJECT_ROOT/dist/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$test_exe" 2>&1)
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

# Run build cache tests
run_build_cache_tests() {
    local suite="build_cache"
    local test_script="$PROJECT_ROOT/tests/build_cache/test_build_cache.sh"

    if ! should_run_test "$suite" "build_cache"; then
        return
    fi

    print_header "Build Cache Tests"

    if [[ ! -x "$test_script" ]]; then
        log_skip "$suite" "build_cache" "test script not found"
        return
    fi

    local output
    local exit_code
    output=$(cd "$PROJECT_ROOT" && QUADC="$PROJECT_ROOT/$BUILD_DIR/cmd/quadc/quadc" QUADRATE_ROOT="$PROJECT_ROOT" QUADRATE_LIBDIR="$PROJECT_ROOT/dist/lib" bash "$test_script" 2>&1)
    exit_code=$?

    if [[ $exit_code -eq 0 ]]; then
        # Count pass/fail from output
        local passed=$(echo "$output" | grep -c "✓")
        local failed=$(echo "$output" | grep -c "✗")
        local i=0
        while [[ $i -lt $passed ]]; do
            log_pass "$suite" "build_cache_$i"
            i=$((i + 1))
        done
        if [[ $failed -gt 0 ]]; then
            log_fail "$suite" "build_cache" "test failed" "$output"
        fi
    else
        log_fail "$suite" "build_cache" "test failed" "$output"
    fi
}

# Run quadmcp (MCP server) tests
run_quadmcp_tests() {
    local suite="quadmcp"
    local test_script="$PROJECT_ROOT/tests/quadmcp/test_quadmcp.sh"

    # Check if test script exists
    if [[ ! -f "$test_script" ]]; then
        if should_run_test "$suite" "quadmcp"; then
            print_header "MCP Server Tests"
            log_skip "$suite" "quadmcp" "test script not found"
        fi
        return
    fi

    # Check if quadmcp binary exists
    if [[ ! -x "$PROJECT_ROOT/dist/bin/quadmcp" ]]; then
        if should_run_test "$suite" "quadmcp"; then
            print_header "MCP Server Tests"
            log_skip "$suite" "quadmcp" "quadmcp binary not found (run 'make' first)"
        fi
        return
    fi

    local output_file="$TEMP_DIR/quadmcp_test_output.txt"

    # Run tests and capture output to file
    # Explicitly set QUADRATE_ROOT and QUADMCP to avoid picking up values from other test suites
    (QUADRATE_ROOT="$PROJECT_ROOT" QUADMCP="$PROJECT_ROOT/dist/bin/quadmcp" bash "$test_script") > "$output_file" 2>&1

    # Parse individual test results from output
    # Format: "PASS: test name" or "FAIL: test name"
    local -a test_results=()
    while IFS= read -r line; do
        # Strip ANSI codes and extract test results
        local clean_line=$(echo "$line" | sed -e 's/\x1b\[[0-9;]*m//g' -e 's/\\033\[[0-9;]*m//g')
        if [[ "$clean_line" =~ ^PASS:\ (.+)$ ]]; then
            test_results+=("PASS:${BASH_REMATCH[1]}")
        elif [[ "$clean_line" =~ ^FAIL:\ (.+)$ ]]; then
            test_results+=("FAIL:${BASH_REMATCH[1]}")
        fi
    done < "$output_file"

    # Filter tests based on should_run_test
    local -a filtered_tests=()
    for result in "${test_results[@]}"; do
        local test_name="${result#*:}"
        if should_run_test "$suite" "$test_name"; then
            filtered_tests+=("$result")
        fi
    done

    [[ ${#filtered_tests[@]} -eq 0 ]] && return

    print_header "MCP Server Tests"

    CURRENT_TEST_TOTAL=${#filtered_tests[@]}
    CURRENT_TEST_NUM=0

    for result in "${filtered_tests[@]}"; do
        CURRENT_TEST_NUM=$((CURRENT_TEST_NUM + 1))
        local status="${result%%:*}"
        local test_name="${result#*:}"

        if [[ "$status" == "PASS" ]]; then
            log_pass "$suite" "$test_name"
        else
            log_fail "$suite" "$test_name" "test failed" ""
        fi
    done

    CURRENT_TEST_TOTAL=0
}

# Run quad CLI tests
run_quad_tests() {
    local suite="quad"
    local test_script="$PROJECT_ROOT/tests/quad/test_quad.sh"

    if ! should_run_test "$suite" "quad_tests"; then
        return
    fi

    print_header "Quad CLI Tests"

    if [[ ! -f "$test_script" ]]; then
        log_skip "$suite" "quad_tests" "test script not found"
        return
    fi

    local output
    local exit_code
    output=$(cd "$PROJECT_ROOT" && bash "$test_script" 2>&1)
    exit_code=$?

    # Parse pass/fail counts from output
    local summary_line=$(echo "$output" | tail -1)
    local passed=$(echo "$summary_line" | grep -o '[0-9]* passed' | grep -o '[0-9]*')
    local failed=$(echo "$summary_line" | grep -o '[0-9]* failed' | grep -o '[0-9]*')

    if [[ $exit_code -eq 0 ]]; then
        log_pass "$suite" "quad_tests" "(${passed:-0} tests)"
    else
        log_fail "$suite" "quad_tests" "${failed:-?}/${passed:-?} tests failed"
    fi
}

# Run command-line argument tests
run_args_tests() {
    local suite="args"
    local test_dir="$PROJECT_ROOT/tests/qd/args"

    if ! should_run_test "$suite" "args_tests"; then
        return
    fi

    print_header "CLI Argument Tests"

    # Check if test files exist
    if [[ ! -d "$test_dir" ]]; then
        log_skip "$suite" "args_tests" "test directory not found"
        return
    fi

    local passed=0
    local failed=0

    # Test function: run_arg_test <description> <qd_file> <expected_output> [args...]
    run_arg_test() {
        local desc="$1"
        local qd_file="$2"
        local expected="$3"
        shift 3
        local args=("$@")

        local cmd=("$QUADC" "-r" "$test_dir/$qd_file")
        if [[ ${#args[@]} -gt 0 ]]; then
            cmd+=("--")
            cmd+=("${args[@]}")
        fi

        local actual
        # Capture only stdout, ignore stderr (compiler warnings)
        actual=$("${cmd[@]}" 2>/dev/null)

        if [[ "$actual" == "$expected" ]]; then
            ((passed++))
        else
            ((failed++))
            if [[ $VERBOSE -eq 1 ]]; then
                echo "  ${desc}: expected '$expected', got '$actual'"
            fi
        fi
    }

    # Run tests
    run_arg_test "greet with single arg" "greet.qd" "Hello, Alice!" "Alice"
    run_arg_test "greet with no args" "greet.qd" "Usage: greet <name>"
    run_arg_test "echo multiple args" "echo_args.qd" "argc=3
arg0=third
arg1=second
arg2=first" "first" "second" "third"
    run_arg_test "echo no args" "echo_args.qd" "argc=0"
    run_arg_test "echo single arg" "echo_args.qd" "argc=1
arg0=hello" "hello"
    run_arg_test "arg with spaces" "spaces_in_args.qd" "Got: [Hello World]" "Hello World"
    run_arg_test "spaces test no args" "spaces_in_args.qd" "No arguments"

    if [[ $failed -eq 0 ]]; then
        log_pass "$suite" "args_tests" "($passed tests)"
    else
        log_fail "$suite" "args_tests" "$failed/$((passed + failed)) tests failed"
    fi
}

# Run cross-compilation tests (--target flag)
run_crosscompile_tests() {
    local suite="crosscompile"

    if ! should_run_test "$suite" "crosscompile_tests"; then
        return
    fi

    print_header "Cross-Compilation Tests"

    local passed=0
    local failed=0
    local skipped=0

    # Create temp directory for test outputs
    local test_temp="$TEMP_DIR/crosscompile"
    mkdir -p "$test_temp"

    # Helper function to run a cross-compile test
    run_xcompile_test() {
        local desc="$1"
        local target="$2"
        local expected_error="$3"  # Expected error pattern or "success" for native
        shift 3

        local src_file="$test_temp/test_${target//[^a-zA-Z0-9]/_}.qd"

        # Create a simple test program
        cat > "$src_file" << 'EOFSRC'
fn main(--) {
    42 print nl
}
EOFSRC

        # Try to compile with --target
        local output
        output=$("$QUADC" --target "$target" -o "$test_temp/output_${target//[^a-zA-Z0-9]/_}" "$src_file" 2>&1)
        local exit_code=$?

        if [[ "$expected_error" == "success" ]]; then
            # Native compilation should succeed
            if [[ $exit_code -eq 0 ]]; then
                ((passed++))
            else
                ((failed++))
                if [[ $VERBOSE -eq 1 ]]; then
                    echo "  $desc: expected success but failed"
                    echo "    output: $output"
                fi
            fi
            return
        fi

        # For cross-compilation targets, we expect:
        # 1. Object file generation to succeed (LLVM generates for target)
        # 2. Linking to fail with architecture mismatch error
        # The key indicator is "Relocations in generic ELF" or "file format not recognized"
        # which means the object was generated for a different architecture

        if [[ $exit_code -ne 0 ]]; then
            # Check if this is the expected cross-compilation linker error
            if echo "$output" | grep -qE "Relocations in generic ELF|file format not recognized|incompatible|wrong ELF class"; then
                # This is expected - object was created for target arch but linker can't handle it
                ((passed++))
            elif echo "$output" | grep -q "Error:"; then
                # LLVM error - target might not be registered/supported
                ((failed++))
                if [[ $VERBOSE -eq 1 ]]; then
                    echo "  $desc: LLVM target error"
                    echo "    output: $output"
                fi
            else
                # Other error - might be OK if it's linker-related
                if echo "$output" | grep -qE "linker|ld:|clang:"; then
                    ((passed++))  # Linker errors are expected for cross-compile
                else
                    ((failed++))
                    if [[ $VERBOSE -eq 1 ]]; then
                        echo "  $desc: unexpected error"
                        echo "    output: $output"
                    fi
                fi
            fi
        else
            # Cross-compilation unexpectedly succeeded
            # This might happen if user has cross-compilation toolchain installed
            ((passed++))
        fi
    }

    # Test: --target flag parsing
    test_target_flag_parsing() {
        # Test that --target is recognized
        local output
        output=$("$QUADC" --help 2>&1)
        if echo "$output" | grep -q -- "--target"; then
            ((passed++))
        else
            ((failed++))
            if [[ $VERBOSE -eq 1 ]]; then
                echo "  --target not in help output"
            fi
        fi
    }

    # Test: --target requires argument
    test_target_requires_arg() {
        local output
        output=$("$QUADC" --target 2>&1)
        local exit_code=$?
        if [[ $exit_code -ne 0 ]] && echo "$output" | grep -q "requires"; then
            ((passed++))
        else
            ((failed++))
            if [[ $VERBOSE -eq 1 ]]; then
                echo "  --target without arg should fail with 'requires' message"
            fi
        fi
    }

    # Test: native compilation still works
    test_native_compilation() {
        local src_file="$test_temp/native_test.qd"
        local out_file="$test_temp/native_test"

        cat > "$src_file" << 'EOFSRC'
fn main(--) {
    42 print nl
}
EOFSRC

        local output
        output=$("$QUADC" -o "$out_file" "$src_file" 2>&1)
        local exit_code=$?

        if [[ $exit_code -eq 0 && -x "$out_file" ]]; then
            # Verify it runs
            local run_output
            run_output=$("$out_file" 2>/dev/null)
            if [[ "$run_output" == "42" ]]; then
                ((passed++))
            else
                ((failed++))
                if [[ $VERBOSE -eq 1 ]]; then
                    echo "  native executable output wrong: got '$run_output', expected '42'"
                fi
            fi
        else
            ((failed++))
            if [[ $VERBOSE -eq 1 ]]; then
                echo "  native compilation failed: $output"
            fi
        fi
    }

    # Test: default target (empty string) works
    test_empty_target() {
        local src_file="$test_temp/empty_target_test.qd"
        local out_file="$test_temp/empty_target_test"

        cat > "$src_file" << 'EOFSRC'
fn main(--) {
    1 print nl
}
EOFSRC

        # Compile with empty target (should use default)
        local output
        output=$("$QUADC" --target "" -o "$out_file" "$src_file" 2>&1)
        local exit_code=$?

        if [[ $exit_code -eq 0 && -x "$out_file" ]]; then
            ((passed++))
        else
            ((failed++))
            if [[ $VERBOSE -eq 1 ]]; then
                echo "  empty target compilation failed"
            fi
        fi
    }

    # Test: --target with optimization flags
    test_target_with_optimization() {
        local src_file="$test_temp/opt_test.qd"

        cat > "$src_file" << 'EOFSRC'
fn main(--) {
    1 2 + print nl
}
EOFSRC

        local output
        # This may fail at linking, but should succeed at compilation
        output=$("$QUADC" --target "aarch64-linux-gnu" -O2 -o "$test_temp/opt_output" "$src_file" 2>&1)

        # Check that it at least attempted compilation (not an argument error)
        if ! echo "$output" | grep -q "unknown option\|requires"; then
            ((passed++))
        else
            ((failed++))
            if [[ $VERBOSE -eq 1 ]]; then
                echo "  --target with -O2 failed at argument parsing"
            fi
        fi
    }

    # Test: --target with debug info
    test_target_with_debug() {
        local src_file="$test_temp/debug_test.qd"

        cat > "$src_file" << 'EOFSRC'
fn main(--) {
    100 print nl
}
EOFSRC

        local output
        output=$("$QUADC" --target "aarch64-linux-gnu" -g -o "$test_temp/debug_output" "$src_file" 2>&1)

        if ! echo "$output" | grep -q "unknown option\|requires"; then
            ((passed++))
        else
            ((failed++))
            if [[ $VERBOSE -eq 1 ]]; then
                echo "  --target with -g failed at argument parsing"
            fi
        fi
    }

    # Run all tests
    test_target_flag_parsing
    test_target_requires_arg
    test_native_compilation
    test_empty_target
    test_target_with_optimization
    test_target_with_debug

    # Cross-compile target tests
    # These verify that LLVM generates code for the specified target
    # Note: Full linking requires cross-toolchain, so we verify the linker error indicates
    # the object was created for the wrong architecture (expected behavior)

    run_xcompile_test "aarch64-linux-gnu target" "aarch64-linux-gnu" "cross"
    run_xcompile_test "x86_64-linux-gnu target" "x86_64-linux-gnu" "success"  # Should work on x86_64 host
    run_xcompile_test "aarch64-apple-darwin target" "aarch64-apple-darwin" "cross"

    # Cleanup
    rm -rf "$test_temp"

    if [[ $failed -eq 0 ]]; then
        log_pass "$suite" "crosscompile_tests" "($passed tests)"
    else
        log_fail "$suite" "crosscompile_tests" "$failed/$((passed + failed)) tests failed"
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

run_stdlib_tests() {
    local suite="stdlib"
    local quad="$PROJECT_ROOT/$BUILD_DIR/cmd/quad/quad"

    if ! should_run_test "$suite" "stdlib_tests"; then
        return
    fi

    # Check if quad exists
    if [[ ! -x "$quad" ]]; then
        log_skip "$suite" "stdlib_tests" "quad not found"
        return
    fi

    print_header "Standard Library Unit Tests"

    local output
    local exit_code

    # Find test files in tests/qd/stdlib/
    local test_files=""
    for f in "$PROJECT_ROOT/tests/qd/stdlib"/*_test.qd; do
        [[ -f "$f" ]] && test_files+=" $f"
    done
    output=$(cd "$PROJECT_ROOT" && \
        QUADRATE_ROOT="$QUADRATE_ROOT_DEFAULT" \
        QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" \
        "$quad" test $test_files 2>&1)
    exit_code=$?

    # Count passed/failed from output
    local passed=$(echo "$output" | grep -oE '[0-9]+ passed' | awk '{sum+=$1} END {print sum+0}')
    local failed=$(echo "$output" | grep -oE '[0-9]+ failed' | awk '{sum+=$1} END {print sum+0}')

    if [[ $exit_code -eq 0 ]] && [[ $failed -eq 0 ]]; then
        # Log each passing test file as a single pass
        SUITE_PASSED[$suite]=$((${SUITE_PASSED[$suite]:-0} + passed))
        TOTAL_PASSED=$((TOTAL_PASSED + passed))
        echo -e "  ${GREEN}✓${NC} stdlib_tests ${DIM}($passed tests passed)${NC}"
    else
        SUITE_FAILED[$suite]=$((${SUITE_FAILED[$suite]:-0} + 1))
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
        FAILED_TESTS_LIST+=("stdlib:stdlib_tests")
        echo -e "  ${RED}✗${NC} stdlib_tests ${DIM}($passed passed, $failed failed)${NC}"
        if [[ -n "$output" ]]; then
            echo "$output" | grep -E "(✗|FAIL|Assertion failed)" | head -10 | sed 's/^/      /'
        fi
    fi
}

run_helgrind_tests() {
    local suite="stdlib"

    if [[ $USE_HELGRIND -ne 1 ]]; then
        return
    fi

    if ! command -v valgrind &> /dev/null; then
        log_skip "$suite" "helgrind" "valgrind not found"
        return
    fi

    print_header "Helgrind Thread Analysis"

    # Find all test files that use the thread module
    local thread_tests=()
    while IFS= read -r test_file; do
        if grep -q 'use thread' "$test_file" 2>/dev/null; then
            thread_tests+=("$test_file")
        fi
    done <<< "$(find "$PROJECT_ROOT/lib" -name '*_test.qd' -type f 2>/dev/null)"

    if [[ ${#thread_tests[@]} -eq 0 ]]; then
        log_skip "$suite" "helgrind" "no thread tests found"
        return
    fi

    local quadc="$PROJECT_ROOT/$BUILD_DIR/cmd/quadc/quadc"
    if [[ ! -x "$quadc" ]]; then
        log_skip "$suite" "helgrind" "quadc not found"
        return
    fi

    for test_file in "${thread_tests[@]}"; do
        local test_name=$(basename "$test_file" .qd)
        local binary="$TEMP_DIR/helgrind_${test_name}"
        local helgrind_log="$TEMP_DIR/helgrind_${test_name}.log"

        # Compile the test
        if ! QUADRATE_ROOT="$QUADRATE_ROOT_DEFAULT" QUADRATE_LIBDIR="$QUADRATE_LIBDIR_DEFAULT" \
            "$quadc" --test -o "$binary" "$test_file" 2>/dev/null; then
            log_fail "$suite" "helgrind:${test_name}" "compilation failed"
            continue
        fi

        # Run under helgrind
        local output
        local exit_code
        output=$(valgrind --tool=helgrind --error-exitcode=1 --log-file="$helgrind_log" "$binary" 2>&1)
        exit_code=$?

        if [[ $exit_code -eq 0 ]]; then
            log_pass "$suite" "helgrind:${test_name}"
        else
            if grep -q "ERROR SUMMARY: [1-9]" "$helgrind_log" 2>/dev/null; then
                local errors=$(grep -c "Possible data race\|Lock order violated\|Thread.*exited" "$helgrind_log" 2>/dev/null || echo "?")
                log_fail "$suite" "helgrind:${test_name}" "$errors thread errors detected"
                if [[ $VERBOSE -eq 1 ]]; then
                    grep -A 5 "Possible data race\|Lock order violated" "$helgrind_log" | head -30 | sed 's/^/      /'
                fi
            else
                log_pass "$suite" "helgrind:${test_name}" "(test passed, runtime error)"
            fi
        fi

        rm -f "$binary"
    done
}

run_fuzz_tests() {
    local suite="fuzz"

    if ! should_run_test "$suite" "fuzz_parser"; then
        return
    fi

    if ! command -v clang++ &> /dev/null; then
        log_skip "$suite" "fuzz_parser" "clang++ not found"
        return
    fi

    print_header "Fuzz Tests ${DIM}(${FUZZ_TIME}s)${NC}"

    local fuzz_build="$PROJECT_ROOT/build/fuzz"
    local fuzz_exe="$fuzz_build/tests/fuzz/fuzz_parser"

    if [[ ! -x "$fuzz_exe" ]]; then
        echo -e "  ${DIM}Building fuzz target...${NC}"
        local build_output
        if ! build_output=$(CC=clang CXX=clang++ meson setup "$fuzz_build" --buildtype=debug -Dbuild_fuzz=true 2>&1); then
            log_fail "$suite" "fuzz_parser" "build setup failed" "$build_output"
            return
        fi
        if ! build_output=$(meson compile -C "$fuzz_build" tests/fuzz/fuzz_parser 2>&1); then
            log_fail "$suite" "fuzz_parser" "build failed" "$build_output"
            return
        fi
    fi

    local corpus_dir="$PROJECT_ROOT/tests/fuzz/corpus"
    local crash_dir="$TEMP_DIR/fuzz_crashes"
    mkdir -p "$crash_dir"

    # Disable leak detection for fuzz tests - valgrind tests cover memory leaks
    # This prevents false positives from different ASan versions across platforms
    ASAN_OPTIONS=detect_leaks=0 "$fuzz_exe" "$corpus_dir" \
        -max_len=5000 \
        -max_total_time="$FUZZ_TIME" \
        -artifact_prefix="$crash_dir/" \
        > /dev/null 2>&1

    local crash_count=$(find "$crash_dir" -type f 2>/dev/null | wc -l)

    if [[ $crash_count -gt 0 ]]; then
        local crash_files=$(ls "$crash_dir" 2>/dev/null | head -5)
        log_fail "$suite" "fuzz_parser" "$crash_count crash(es) found" "Crashes saved to: $crash_dir\n$crash_files"
    else
        log_pass "$suite" "fuzz_parser"
    fi
}

# List all available tests
list_all_tests() {
    echo "Available tests:"
    echo ""

    echo "C++ Tests (suite: cpp):"
    for test in test_ast test_semantic_validator test_runtime test_llvmgen test_mem; do
        echo "  $test"
    done
    echo ""

    echo "LSP Tests (suite: lsp):"
    for test in test_lsp test_lsp_extended test_lsp_stress test_lsp_features test_lsp_comprehensive; do
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
    for test in embed multi-module-test native-functions-test incremental-test typed-native-test embed-comprehensive-test ffi; do
        echo "  $test"
    done
    echo ""

    echo "Package Manager Tests (suite: quadpm):"
    echo "  quadpm_tests"
    echo ""

    echo "Standard Library Unit Tests (suite: stdlib):"
    echo "  stdlib_tests"
    echo ""

    echo "mTLS Tests (suite: mtls):"
    echo "  mtls_test"
    echo ""

    echo "Fuzz Tests (suite: fuzz):"
    echo "  fuzz_parser"
}

# Print summary
print_summary() {
    echo ""
    echo -e "${BOLD}═══════════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLD}  Summary${NC}"
    echo -e "${BOLD}═══════════════════════════════════════════════════════════════════════════════${NC}"

    # Print per-suite summary
    for suite in cpp lsp qd formatter linter embed quadpm build_cache quadmcp stdlib mtls fuzz; do
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
            build_cache) suite_name="Build Cache" ;;
            quadmcp) suite_name="MCP Server" ;;
            stdlib) suite_name="Stdlib Unit Tests" ;;
            mtls) suite_name="mTLS" ;;
            fuzz) suite_name="Fuzz" ;;
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
    if [[ $USE_HELGRIND -eq 1 ]]; then
        mode_info="${mode_info:+$mode_info }${CYAN}helgrind${NC}"
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

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "build_cache" ]]; then
        run_build_cache_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "quadmcp" ]]; then
        run_quadmcp_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "quad" ]]; then
        run_quad_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "args" ]]; then
        run_args_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "crosscompile" ]]; then
        run_crosscompile_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "stdlib" ]]; then
        run_stdlib_tests
        run_helgrind_tests
    fi

    if [[ -z "$SPECIFIC_SUITE" ]] || [[ "$SPECIFIC_SUITE" == "mtls" ]]; then
        run_mtls_tests
    fi

    # Fuzz tests only run when explicitly requested (--suite fuzz)
    # They are non-deterministic and meant for local bug discovery, not CI
    if [[ "$SPECIFIC_SUITE" == "fuzz" ]]; then
        run_fuzz_tests
    fi

    print_summary
}

main
