#!/bin/bash

# Formatter test runner for Quadrate
# Tests that quadfmt properly formats code

set -e

# Get script directory and source shared utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_utils.sh"

# Directories
TEST_DIR="tests/formatter"
EXPECTED_DIR="tests/formatter/expected"
TEMP_DIR=$(mktemp -d)
QUADFMT="${QUADFMT:-dist/bin/quadfmt}"

# Cleanup on exit
trap "rm -rf $TEMP_DIR" EXIT

print_header "Quadrate Formatter Test Suite"

# Function to run a single formatter test
run_formatter_test() {
	local test_file="$1"
	local test_name=$(basename "$test_file" .qd)
	local expected_file="$EXPECTED_DIR/${test_name}.qd"
	local output_file="$TEMP_DIR/${test_name}.qd"

	increment_test_counter

	# Check if expected output exists
	if [ ! -f "$expected_file" ]; then
		log_skip "$test_name" "no expected output file"
		return
	fi

	# Run formatter
	if ! "$QUADFMT" -w "$test_file" > "$output_file" 2>"$TEMP_DIR/${test_name}.err"; then
		log_fail "$test_name" "formatter failed"
		cat "$TEMP_DIR/${test_name}.err"
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

# Function to test idempotency (formatting twice gives same result)
run_idempotency_test() {
	local test_file="$1"
	local test_name=$(basename "$test_file" .qd)
	local first_format="$TEMP_DIR/${test_name}_1.qd"
	local second_format="$TEMP_DIR/${test_name}_2.qd"

	increment_test_counter

	# Format once
	if ! "$QUADFMT" -w "$test_file" > "$first_format" 2>/dev/null; then
		log_skip "${test_name}_idempotent" "formatter failed"
		return
	fi

	# Format the formatted output
	if ! "$QUADFMT" -w "$first_format" > "$second_format" 2>/dev/null; then
		log_fail "${test_name}_idempotent" "second format failed"
		return
	fi

	# Compare
	if diff -q "$first_format" "$second_format" > /dev/null 2>&1; then
		log_pass "${test_name}_idempotent"
	else
		log_fail "${test_name}_idempotent" "not idempotent"
		print_indented "First format" "$first_format" 20
		print_indented "Second format" "$second_format" 20
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

# Print summary and exit
print_summary
print_result_and_exit
