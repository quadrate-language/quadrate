#!/bin/bash

# Shared test utilities for Quadrate test scripts
# Source this file in test runners to get common logging and formatting functions

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# Logging functions
log_pass() {
	local test_name="$1"
	echo -e "${GREEN}PASS${NC}  $test_name"
	TESTS_PASSED=$((TESTS_PASSED + 1))
}

log_fail() {
	local test_name="$1"
	local reason="$2"
	if [ -n "$reason" ]; then
		echo -e "${RED}FAIL${NC}  $test_name ($reason)"
	else
		echo -e "${RED}FAIL${NC}  $test_name"
	fi
	TESTS_FAILED=$((TESTS_FAILED + 1))
}

log_skip() {
	local test_name="$1"
	local reason="$2"
	if [ -n "$reason" ]; then
		echo -e "${YELLOW}SKIP${NC}  $test_name ($reason)"
	else
		echo -e "${YELLOW}SKIP${NC}  $test_name"
	fi
	TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
}

log_info() {
	local message="$1"
	echo -e "${BLUE}INFO${NC}  $message"
}

# Print indented content (for showing diffs, expected vs actual, etc.)
print_indented() {
	local label="$1"
	local file="$2"
	local max_lines="${3:-0}"  # 0 means no limit

	echo "  $label:"
	if [ "$max_lines" -gt 0 ]; then
		sed 's/^/    /' "$file" | head -n "$max_lines"
	else
		sed 's/^/    /' "$file"
	fi
}

# Print diff between two files with indentation
print_diff() {
	local expected="$1"
	local actual="$2"
	echo "  Diff:"
	diff -u "$expected" "$actual" | sed 's/^/    /' || true
}

# Print test suite header
print_header() {
	local suite_name="$1"
	echo "================================================"
	echo "  $suite_name"
	echo "================================================"
	echo ""
}

# Print test summary
print_summary() {
	echo ""
	echo "================================================"
	echo "  Test Summary"
	echo "================================================"
	echo "Tests run:    $TESTS_RUN"
	echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"

	if [ "$TESTS_SKIPPED" -gt 0 ]; then
		echo -e "Tests skipped: ${YELLOW}$TESTS_SKIPPED${NC}"
	fi

	echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
	echo ""
}

# Print final result and exit
print_result_and_exit() {
	if [ "$TESTS_FAILED" -eq 0 ]; then
		echo -e "${GREEN}All tests passed!${NC}"
		exit 0
	else
		echo -e "${RED}Some tests failed!${NC}"
		exit 1
	fi
}

# Increment test counter
increment_test_counter() {
	TESTS_RUN=$((TESTS_RUN + 1))
}

# Reset all counters (useful if running multiple test suites in one script)
reset_counters() {
	TESTS_RUN=0
	TESTS_PASSED=0
	TESTS_FAILED=0
	TESTS_SKIPPED=0
}
