#!/bin/bash

# Test suite for the quad unified CLI

set -u

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Get quad binary path
QUAD="${QUAD:-dist/bin/quad}"
if [[ "$QUAD" != /* ]]; then
    QUAD="$(pwd)/$QUAD"
fi

# Temporary directory for tests
TEST_DIR="/tmp/quad_test_$$"
mkdir -p "$TEST_DIR"

cleanup() {
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

pass() {
    echo -e "${GREEN}✓${NC} $1"
    ((TESTS_PASSED++))
    ((TESTS_RUN++))
}

fail() {
    echo -e "${RED}✗${NC} $1"
    echo "  $2"
    ((TESTS_FAILED++))
    ((TESTS_RUN++))
}

# Check if quad binary exists
if [ ! -x "$QUAD" ]; then
    echo -e "${RED}Error: quad binary not found at $QUAD${NC}"
    exit 1
fi

echo "Testing quad CLI..."
echo "Binary: $QUAD"
echo ""

# ---- Version / Help ----

output=$("$QUAD" version 2>&1)
if echo "$output" | grep -qE "quad [0-9]+\.[0-9]+"; then
    pass "quad version shows version"
else
    fail "quad version shows version" "got: $output"
fi

output=$("$QUAD" --help 2>&1)
if echo "$output" | grep -q "Usage:.*quad"; then
    pass "quad --help shows usage"
else
    fail "quad --help shows usage" "got: $output"
fi

output=$("$QUAD" help 2>&1)
if echo "$output" | grep -q "Usage:.*quad"; then
    pass "quad help shows usage"
else
    fail "quad help shows usage" "got: $output"
fi

# ---- Build / Run ----
# Each test uses its own subdirectory to avoid sibling .qd file auto-discovery
# causing "Duplicate function definition: 'main'" errors.

mkdir -p "$TEST_DIR/run"
cat > "$TEST_DIR/run/hello.qd" <<'EOF'
fn main() {
    "hello quad" print nl
}
EOF

output=$("$QUAD" run "$TEST_DIR/run/hello.qd" 2>/dev/null)
if [[ "$output" == "hello quad" ]]; then
    pass "quad run executes program"
else
    fail "quad run executes program" "got: $output"
fi

mkdir -p "$TEST_DIR/build"
cat > "$TEST_DIR/build/hello.qd" <<'EOF'
fn main() {
    "hello quad" print nl
}
EOF

"$QUAD" build "$TEST_DIR/build/hello.qd" -o "$TEST_DIR/build/hello" 2>/dev/null
if [ -x "$TEST_DIR/build/hello" ]; then
    pass "quad build creates executable"
    output=$("$TEST_DIR/build/hello" 2>/dev/null)
    if [[ "$output" == "hello quad" ]]; then
        pass "built executable runs correctly"
    else
        fail "built executable runs correctly" "got: $output"
    fi
else
    fail "quad build creates executable" "binary not found"
fi

# ---- Run with arguments ----

mkdir -p "$TEST_DIR/args"
cat > "$TEST_DIR/args/args.qd" <<'EOF'
fn main() {
    read -> argc
    argc print nl
}
EOF

output=$("$QUAD" run "$TEST_DIR/args/args.qd" -- foo bar 2>/dev/null)
if [[ "$output" == "2" ]]; then
    pass "quad run passes arguments"
else
    fail "quad run passes arguments" "got: $output"
fi

# ---- Test command ----

cat > "$TEST_DIR/sample_test.qd" <<'EOF'
use testing

test "simple pass" {
    1 1 testing::assert_eq
}
EOF

output=$("$QUAD" test "$TEST_DIR/sample_test.qd" 2>/dev/null)
if echo "$output" | grep -q "1 passed"; then
    pass "quad test runs tests"
else
    fail "quad test runs tests" "got: $output"
fi

# ---- Format command ----

cat > "$TEST_DIR/unformatted.qd" <<'EOF'
fn   add_two( x:i64  --  r:i64 )  {
    x    2   +
}
EOF
cp "$TEST_DIR/unformatted.qd" "$TEST_DIR/unformatted_backup.qd"

"$QUAD" fmt "$TEST_DIR/unformatted.qd" 2>/dev/null
if ! diff -q "$TEST_DIR/unformatted.qd" "$TEST_DIR/unformatted_backup.qd" >/dev/null 2>&1; then
    pass "quad fmt modifies file"
else
    fail "quad fmt modifies file" "file unchanged"
fi

# ---- Init command ----

mkdir -p "$TEST_DIR/new_project"
(cd "$TEST_DIR/new_project" && "$QUAD" init 2>/dev/null)
if [ -f "$TEST_DIR/new_project/main.qd" ]; then
    pass "quad init creates main.qd"
else
    fail "quad init creates main.qd" "main.qd not found"
fi

# ---- Unknown command ----

output=$("$QUAD" nonexistent 2>&1)
rc=$?
if [ $rc -ne 0 ]; then
    pass "quad rejects unknown command"
else
    fail "quad rejects unknown command" "exit code was 0"
fi

# ---- Direct script execution (shebang mode) ----

mkdir -p "$TEST_DIR/direct"
cat > "$TEST_DIR/direct/direct.qd" <<'EOFDIRECT'
fn main() {
    "direct mode" print nl
}
EOFDIRECT

output=$("$QUAD" "$TEST_DIR/direct/direct.qd" 2>/dev/null)
if [[ "$output" == "direct mode" ]]; then
    pass "quad file.qd runs directly"
else
    fail "quad file.qd runs directly" "got: $output"
fi

# ---- Summary ----

echo ""
echo "$TESTS_RUN tests: $TESTS_PASSED passed, $TESTS_FAILED failed"

if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
fi
exit 0
