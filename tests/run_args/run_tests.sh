#!/bin/bash
# Test script for command-line argument passing via --

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Configuration
QUADC="${QUADC:-$PROJECT_ROOT/build/debug/cmd/quadc/quadc}"
QUAD="${QUAD:-$PROJECT_ROOT/dist/bin/quad}"
QUADRATE_ROOT="${QUADRATE_ROOT:-$PROJECT_ROOT/dist/share/quadrate}"
QUADRATE_LIBDIR="${QUADRATE_LIBDIR:-$PROJECT_ROOT/dist/lib}"

export QUADRATE_ROOT
export QUADRATE_LIBDIR

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TESTS_PASSED=0
TESTS_FAILED=0

log_pass() {
    echo -e "${GREEN}PASS${NC}  $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

log_fail() {
    echo -e "${RED}FAIL${NC}  $1: $2"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

# Test function: run_test <description> <qd_file> <expected_output> [args...]
run_test() {
    local desc="$1"
    local qd_file="$2"
    local expected="$3"
    shift 3
    local args=("$@")

    # Build the command with -- separator
    local cmd=("$QUADC" "-r" "$SCRIPT_DIR/$qd_file")
    if [ ${#args[@]} -gt 0 ]; then
        cmd+=("--")
        cmd+=("${args[@]}")
    fi

    # Run and capture output
    local actual
    actual=$("${cmd[@]}" 2>&1)
    local exit_code=$?

    if [ "$actual" = "$expected" ]; then
        log_pass "$desc"
    else
        log_fail "$desc" "expected '$expected', got '$actual'"
    fi
}

# Test function for quad command
run_test_quad() {
    local desc="$1"
    local qd_file="$2"
    local expected="$3"
    shift 3
    local args=("$@")

    # Build the command with -- separator
    local cmd=("$QUAD" "run" "$SCRIPT_DIR/$qd_file")
    if [ ${#args[@]} -gt 0 ]; then
        cmd+=("--")
        cmd+=("${args[@]}")
    fi

    # Run and capture output
    local actual
    actual=$("${cmd[@]}" 2>&1)
    local exit_code=$?

    if [ "$actual" = "$expected" ]; then
        log_pass "$desc (quad run)"
    else
        log_fail "$desc (quad run)" "expected '$expected', got '$actual'"
    fi
}

echo "=== Testing command-line argument passing via -- ==="
echo ""

# Test 1: Single argument
run_test "greet with single arg" "greet.qd" "Hello, Alice!" "Alice"

# Test 2: No arguments (should show usage)
run_test "greet with no args" "greet.qd" "Usage: greet <name>"

# Test 3: Multiple arguments (stack order: last arg on top)
run_test "echo multiple args" "echo_args.qd" "argc=3
arg0=third
arg1=second
arg2=first" "first" "second" "third"

# Test 4: No arguments to echo
run_test "echo no args" "echo_args.qd" "argc=0"

# Test 5: Single argument to echo
run_test "echo single arg" "echo_args.qd" "argc=1
arg0=hello" "hello"

# Test 6: Argument with spaces (quoted)
run_test "arg with spaces" "spaces_in_args.qd" "Got: [Hello World]" "Hello World"

# Test 7: No args to spaces test
run_test "spaces test no args" "spaces_in_args.qd" "No arguments"

echo ""

# Test quad command if available
if [ -x "$QUAD" ]; then
    echo "=== Testing quad run with -- ==="
    echo ""

    run_test_quad "greet via quad" "greet.qd" "Hello, Bob!" "Bob"
    run_test_quad "echo via quad" "echo_args.qd" "argc=2
arg0=y
arg1=x" "x" "y"

    echo ""
fi

# Summary
echo "=== Summary ==="
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"

if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
fi
exit 0
