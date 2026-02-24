#!/bin/bash
# Test error handling in quadfmt
# These tests verify that quadfmt correctly handles error conditions

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
QUADFMT="$PROJECT_ROOT/dist/bin/quadfmt"

PASS=0
FAIL=0
TMP_DIR=$(mktemp -d)
trap "rm -rf $TMP_DIR" EXIT

pass() {
    echo -e "  \033[0;32mPASS\033[0m  $1"
    ((PASS++))
}

fail() {
    echo -e "  \033[0;31mFAIL\033[0m  $1: $2"
    ((FAIL++))
}

echo "Formatter Error Handling Tests"
echo ""

# Test 1: Non-existent file
echo "Testing non-existent file..."
if ! "$QUADFMT" /nonexistent/file.qd 2>/dev/null; then
    pass "non_existent_file"
else
    fail "non_existent_file" "should fail for non-existent file"
fi

# Test 2: Syntax error in file (missing function body)
echo "Testing file with syntax errors..."
cat > "$TMP_DIR/syntax_error.qd" << 'EOF'
fn test()
fn another() {
    1 print nl
}
EOF
if ! "$QUADFMT" "$TMP_DIR/syntax_error.qd" 2>/dev/null; then
    pass "syntax_error"
else
    fail "syntax_error" "should fail for file with syntax errors"
fi

# Test 3: Unmatched braces
echo "Testing file with unmatched braces..."
cat > "$TMP_DIR/unmatched_braces.qd" << 'EOF'
fn test() {
    1 print nl
EOF
if ! "$QUADFMT" "$TMP_DIR/unmatched_braces.qd" 2>/dev/null; then
    pass "unmatched_braces"
else
    fail "unmatched_braces" "should fail for file with unmatched braces"
fi

# Test 4: Binary file (invalid UTF-8)
echo "Testing binary file..."
printf '\x00\x01\x02\x03\x04\xff\xfe' > "$TMP_DIR/binary.qd"
if ! "$QUADFMT" "$TMP_DIR/binary.qd" 2>/dev/null; then
    pass "binary_file"
else
    fail "binary_file" "should fail for binary file"
fi

# Test 5: Empty directory argument
echo "Testing with no arguments..."
if ! "$QUADFMT" 2>/dev/null; then
    pass "no_arguments"
else
    fail "no_arguments" "should fail with no arguments"
fi

# Test 6: Unknown option
echo "Testing unknown option..."
if ! "$QUADFMT" --unknown-option file.qd 2>/dev/null; then
    pass "unknown_option"
else
    fail "unknown_option" "should fail for unknown option"
fi

# Test 7: Invalid line width
echo "Testing invalid line width..."
cat > "$TMP_DIR/valid.qd" << 'EOF'
fn main() {
    42 print nl
}
EOF
if "$QUADFMT" --line-width 80 "$TMP_DIR/valid.qd" >/dev/null 2>&1; then
    pass "valid_line_width"
else
    fail "valid_line_width" "should accept valid line width"
fi

# Test 8: Incomplete function signature
echo "Testing incomplete function signature..."
cat > "$TMP_DIR/incomplete_sig.qd" << 'EOF'
fn test(a:i64 -- {
    a print nl
}
EOF
if ! "$QUADFMT" "$TMP_DIR/incomplete_sig.qd" 2>/dev/null; then
    pass "incomplete_signature"
else
    fail "incomplete_signature" "should fail for incomplete signature"
fi

# Test 9: Check mode on unformatted file (unsorted imports)
echo "Testing check mode on unformatted file..."
cat > "$TMP_DIR/unformatted.qd" << 'EOF'
use strings
use io

fn main() {
	42 print nl
}
EOF
if ! "$QUADFMT" -c "$TMP_DIR/unformatted.qd" 2>/dev/null; then
    pass "check_unformatted"
else
    fail "check_unformatted" "should fail check mode on unformatted file"
fi

# Test 10: Check mode on formatted file
echo "Testing check mode on formatted file..."
cat > "$TMP_DIR/formatted.qd" << 'EOF'
fn main() {
	42 print nl
}
EOF
if "$QUADFMT" -c "$TMP_DIR/formatted.qd" 2>/dev/null; then
    pass "check_formatted"
else
    fail "check_formatted" "should pass check mode on formatted file"
fi

# Test 11: Directory with no .qd files
echo "Testing directory with no .qd files..."
mkdir -p "$TMP_DIR/empty_dir"
touch "$TMP_DIR/empty_dir/test.txt"
if "$QUADFMT" "$TMP_DIR/empty_dir" 2>/dev/null; then
    pass "empty_directory"
else
    fail "empty_directory" "should handle directory with no .qd files"
fi

# Test 12: Multiple files
echo "Testing multiple files..."
cat > "$TMP_DIR/file1.qd" << 'EOF'
fn one() {
	1 print nl
}
EOF
cat > "$TMP_DIR/file2.qd" << 'EOF'
fn two() {
	2 print nl
}
EOF
if "$QUADFMT" "$TMP_DIR/file1.qd" "$TMP_DIR/file2.qd" >/dev/null 2>&1; then
    pass "multiple_files"
else
    fail "multiple_files" "should handle multiple files"
fi

# Test 13: Write mode
echo "Testing write mode..."
cat > "$TMP_DIR/to_format.qd" << 'EOF'
use strings
use io

fn main() {
42 print nl
}
EOF
if "$QUADFMT" -w "$TMP_DIR/to_format.qd" >/dev/null 2>&1; then
    # Verify imports were sorted (io before strings)
    if head -1 "$TMP_DIR/to_format.qd" | grep -q "use io"; then
        pass "write_mode"
    else
        fail "write_mode" "file was not actually formatted"
    fi
else
    fail "write_mode" "write mode should work"
fi

echo ""
echo "═══════════════════════════════════════════════════════════════════════════════"
echo "  Results: $PASS passed, $FAIL failed"
echo "═══════════════════════════════════════════════════════════════════════════════"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
