#!/bin/bash

# Formatter test runner for Quadrate
# Tests that quadfmt properly formats code

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Directories
TEST_DIR="tests/formatter"
EXPECTED_DIR="tests/formatter/expected"
TEMP_DIR=$(mktemp -d)
QUADFMT="${QUADFMT:-dist/bin/quadfmt}"

# Counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Cleanup on exit
trap "rm -rf $TEMP_DIR" EXIT

echo "================================================"
echo "  Quadrate Formatter Test Suite"
echo "================================================"
echo ""

# Function to run a single formatter test
run_formatter_test() {
	local test_file="$1"
	local test_name=$(basename "$test_file" .qd)
	local expected_file="$EXPECTED_DIR/${test_name}.qd"
	local output_file="$TEMP_DIR/${test_name}.qd"

	TESTS_RUN=$((TESTS_RUN + 1))

	# Check if expected output exists
	if [ ! -f "$expected_file" ]; then
		echo -e "${YELLOW}SKIP${NC}  $test_name (no expected output file)"
		return
	fi

	# Run formatter
	if ! "$QUADFMT" -w "$test_file" > "$output_file" 2>"$TEMP_DIR/${test_name}.err"; then
		echo -e "${RED}FAIL${NC}  $test_name (formatter failed)"
		cat "$TEMP_DIR/${test_name}.err"
		TESTS_FAILED=$((TESTS_FAILED + 1))
		return
	fi

	# Compare output with expected
	if diff -q "$expected_file" "$output_file" > /dev/null 2>&1; then
		echo -e "${GREEN}PASS${NC}  $test_name"
		TESTS_PASSED=$((TESTS_PASSED + 1))
	else
		echo -e "${RED}FAIL${NC}  $test_name (output mismatch)"
		echo "  Expected:"
		sed 's/^/    /' "$expected_file"
		echo "  Got:"
		sed 's/^/    /' "$output_file"
		echo "  Diff:"
		diff -u "$expected_file" "$output_file" | sed 's/^/    /' || true
		TESTS_FAILED=$((TESTS_FAILED + 1))
	fi
}

# Function to test idempotency (formatting twice gives same result)
run_idempotency_test() {
	local test_file="$1"
	local test_name=$(basename "$test_file" .qd)
	local first_format="$TEMP_DIR/${test_name}_1.qd"
	local second_format="$TEMP_DIR/${test_name}_2.qd"

	TESTS_RUN=$((TESTS_RUN + 1))

	# Format once
	if ! "$QUADFMT" -w "$test_file" > "$first_format" 2>/dev/null; then
		echo -e "${YELLOW}SKIP${NC}  ${test_name}_idempotent (formatter failed)"
		return
	fi

	# Format the formatted output
	if ! "$QUADFMT" -w "$first_format" > "$second_format" 2>/dev/null; then
		echo -e "${RED}FAIL${NC}  ${test_name}_idempotent (second format failed)"
		TESTS_FAILED=$((TESTS_FAILED + 1))
		return
	fi

	# Compare
	if diff -q "$first_format" "$second_format" > /dev/null 2>&1; then
		echo -e "${GREEN}PASS${NC}  ${test_name}_idempotent"
		TESTS_PASSED=$((TESTS_PASSED + 1))
	else
		echo -e "${RED}FAIL${NC}  ${test_name}_idempotent (not idempotent)"
		echo "  First format:"
		sed 's/^/    /' "$first_format" | head -20
		echo "  Second format:"
		sed 's/^/    /' "$second_format" | head -20
		TESTS_FAILED=$((TESTS_FAILED + 1))
	fi
}

# Run basic formatting tests
if [ -d "$TEST_DIR/input" ]; then
	for test_file in "$TEST_DIR/input"/*.qd; do
		[ -f "$test_file" ] || continue
		run_formatter_test "$test_file"
	done
fi

# Run idempotency tests on all existing language tests
echo ""
echo "Running idempotency tests..."
for test_file in tests/qd/*/*.qd tests/qd/*/*/*.qd; do
	[ -f "$test_file" ] || continue
	# Skip module component files
	if [[ "$test_file" == */modules/*/* ]]; then
		continue
	fi
	run_idempotency_test "$test_file"
done

# Print summary
echo ""
echo "================================================"
echo "  Test Summary"
echo "================================================"
echo "Tests run:    $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
echo ""

# Exit with appropriate code
if [ "$TESTS_FAILED" -eq 0 ]; then
	echo -e "${GREEN}All tests passed!${NC}"
	exit 0
else
	echo -e "${RED}Some tests failed!${NC}"
	exit 1
fi
