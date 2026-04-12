#!/bin/bash

# Comprehensive tests for the build cache (incremental compilation)

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

QUADC="${QUADC:-build/debug/cmd/quadc/quadc}"
if [[ "$QUADC" != /* ]]; then
    QUADC="$(pwd)/$QUADC"
fi

# Use an isolated cache directory so we don't pollute the user's real cache
TEST_DIR="/tmp/quadrate_build_cache_test_$$"
CACHE_DIR="$TEST_DIR/cache"
SRC_DIR="$TEST_DIR/src"

cleanup() {
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

pass() {
    echo -e "  ${GREEN}✓${NC} $1"
    ((TESTS_PASSED++))
    ((TESTS_RUN++))
}

fail() {
    echo -e "  ${RED}✗${NC} $1"
    if [[ -n "${2:-}" ]]; then
        echo "    $2"
    fi
    ((TESTS_FAILED++))
    ((TESTS_RUN++))
}

# Set up environment
mkdir -p "$CACHE_DIR" "$SRC_DIR"
export XDG_CACHE_HOME="$CACHE_DIR"
export QUADRATE_ROOT="${QUADRATE_ROOT:-.}"
export QUADRATE_LIBDIR="${QUADRATE_LIBDIR:-dist/lib}"
export QUADC_TIMING=1

if [ ! -x "$QUADC" ]; then
    echo -e "${RED}Error: quadc not found at $QUADC${NC}"
    exit 1
fi

echo "Testing build cache..."
echo "Compiler: $QUADC"
echo "Cache dir: $CACHE_DIR"
echo ""

# ===================================================================
# Test 1: Cache miss on first build
# ===================================================================
cat > "$SRC_DIR/hello.qd" << 'EOF'
fn main() {
    "Hello" print nl
}
EOF

output=$("$QUADC" "$SRC_DIR/hello.qd" -o "$TEST_DIR/hello" 2>&1)
if [[ $? -eq 0 ]]; then
    if echo "$output" | grep -q "total (cached)"; then
        fail "First build should NOT be cached" "Got cache hit on first build"
    else
        pass "First build: cache miss (expected)"
    fi
else
    fail "First build failed" "$output"
fi

# ===================================================================
# Test 2: Cache hit on identical rebuild
# ===================================================================
output=$("$QUADC" "$SRC_DIR/hello.qd" -o "$TEST_DIR/hello2" 2>&1)
if [[ $? -eq 0 ]]; then
    if echo "$output" | grep -q "total (cached)"; then
        pass "Second build: cache hit (expected)"
    else
        fail "Second build should be cached" "No 'total (cached)' in timing output"
    fi
else
    fail "Second build failed" "$output"
fi

# ===================================================================
# Test 3: Cached executable produces correct output
# ===================================================================
actual=$("$TEST_DIR/hello2" 2>&1)
if [[ "$actual" == "Hello" ]]; then
    pass "Cached executable produces correct output"
else
    fail "Cached executable wrong output" "Expected 'Hello', got '$actual'"
fi

# ===================================================================
# Test 4: Cache miss after source change
# ===================================================================
cat > "$SRC_DIR/hello.qd" << 'EOF'
fn main() {
    "Changed" print nl
}
EOF

output=$("$QUADC" "$SRC_DIR/hello.qd" -o "$TEST_DIR/hello3" 2>&1)
if [[ $? -eq 0 ]]; then
    if echo "$output" | grep -q "total (cached)"; then
        fail "Modified source should NOT be cached"
    else
        pass "Modified source: cache miss (expected)"
    fi
else
    fail "Build after modification failed" "$output"
fi

actual=$("$TEST_DIR/hello3" 2>&1)
if [[ "$actual" == "Changed" ]]; then
    pass "Rebuilt executable reflects source change"
else
    fail "Rebuilt executable has stale output" "Expected 'Changed', got '$actual'"
fi

# ===================================================================
# Test 5: Cache hit after reverting source
# ===================================================================
cat > "$SRC_DIR/hello.qd" << 'EOF'
fn main() {
    "Hello" print nl
}
EOF

output=$("$QUADC" "$SRC_DIR/hello.qd" -o "$TEST_DIR/hello4" 2>&1)
if [[ $? -eq 0 ]]; then
    if echo "$output" | grep -q "total (cached)"; then
        pass "Reverted source: cache hit (content-based hashing works)"
    else
        fail "Reverted source should hit cache" "Same content as first build"
    fi
else
    fail "Build after revert failed" "$output"
fi

# ===================================================================
# Test 6: Different optimization levels produce different cache keys
# ===================================================================
output_o0=$("$QUADC" -O0 "$SRC_DIR/hello.qd" -o "$TEST_DIR/hello_o0" 2>&1)
# First O2 build should be a cache miss
output_o2=$("$QUADC" -O2 "$SRC_DIR/hello.qd" -o "$TEST_DIR/hello_o2" 2>&1)
if echo "$output_o2" | grep -q "total (cached)"; then
    fail "Different opt level should NOT hit cache"
else
    pass "Different opt levels produce separate cache entries"
fi

# ===================================================================
# Test 7: Second O2 build hits cache
# ===================================================================
output_o2b=$("$QUADC" -O2 "$SRC_DIR/hello.qd" -o "$TEST_DIR/hello_o2b" 2>&1)
if echo "$output_o2b" | grep -q "total (cached)"; then
    pass "Same opt level on repeat: cache hit"
else
    fail "Same opt level repeat should hit cache"
fi

# ===================================================================
# Test 8: Cache is skipped for --test mode
# ===================================================================
cat > "$SRC_DIR/test_file.qd" << 'EOF'
test "basic" {
    1 1 == if { "PASS" print nl }
}
EOF

output=$("$QUADC" --test "$SRC_DIR/test_file.qd" -o "$TEST_DIR/test_bin" 2>&1)
# Run same test again
output2=$("$QUADC" --test "$SRC_DIR/test_file.qd" -o "$TEST_DIR/test_bin2" 2>&1)
if echo "$output2" | grep -q "total (cached)"; then
    fail "Test mode should bypass cache"
else
    pass "Test mode: cache bypassed (expected)"
fi

# ===================================================================
# Test 9: Cache is skipped for --dump-ast
# ===================================================================
"$QUADC" "$SRC_DIR/hello.qd" -o "$TEST_DIR/hello_dump" 2>&1 > /dev/null
output=$("$QUADC" --dump-ast "$SRC_DIR/hello.qd" -o "$TEST_DIR/hello_dump2" 2>&1)
if echo "$output" | grep -q "total (cached)"; then
    fail "Dump-ast mode should bypass cache"
else
    pass "Dump-ast mode: cache bypassed (expected)"
fi

# ===================================================================
# Test 10: Multi-module project - cache invalidation on dependency change
# ===================================================================
mkdir -p "$SRC_DIR/helper"
cat > "$SRC_DIR/helper/helper.qd" << 'EOF'
pub fn greet() {
    "Hello from helper" print nl
}
EOF

cat > "$SRC_DIR/multi.qd" << 'EOF'
use helper

fn main() {
    helper::greet
}
EOF

# First build
output=$("$QUADC" -I "$SRC_DIR" "$SRC_DIR/multi.qd" -o "$TEST_DIR/multi" 2>&1)
if [[ $? -ne 0 ]]; then
    fail "Multi-module first build failed" "$output"
else
    pass "Multi-module first build succeeds"
fi

# Same build again - should hit cache
output=$("$QUADC" -I "$SRC_DIR" "$SRC_DIR/multi.qd" -o "$TEST_DIR/multi2" 2>&1)
if echo "$output" | grep -q "total (cached)"; then
    pass "Multi-module repeat: cache hit"
else
    fail "Multi-module repeat should hit cache"
fi

# Change the dependency module
cat > "$SRC_DIR/helper/helper.qd" << 'EOF'
pub fn greet() {
    "Updated helper" print nl
}
EOF

output=$("$QUADC" -I "$SRC_DIR" "$SRC_DIR/multi.qd" -o "$TEST_DIR/multi3" 2>&1)
if echo "$output" | grep -q "total (cached)"; then
    fail "Changed dependency should invalidate cache"
else
    pass "Changed dependency: cache miss (expected)"
fi

actual=$("$TEST_DIR/multi3" 2>&1)
if [[ "$actual" == "Updated helper" ]]; then
    pass "Rebuilt executable uses updated dependency"
else
    fail "Executable has stale dependency" "Expected 'Updated helper', got '$actual'"
fi

# ===================================================================
# Test 11: Cache files actually exist on disk
# ===================================================================
cache_count=$(find "$CACHE_DIR" -name "quadrate" -prune -exec find {} -type f \; 2>/dev/null | wc -l)
if [[ $cache_count -gt 0 ]]; then
    pass "Cache files exist on disk ($cache_count entries)"
else
    fail "No cache files found in $CACHE_DIR"
fi

# ===================================================================
# Test 12: Different stack sizes produce different cache keys
# ===================================================================
output_s1=$("$QUADC" -s 512 "$SRC_DIR/hello.qd" -o "$TEST_DIR/hello_s512" 2>&1)
if echo "$output_s1" | grep -q "total (cached)"; then
    fail "Different stack size should NOT hit cache"
else
    pass "Different stack size produces separate cache entry"
fi

# ===================================================================
# Summary
# ===================================================================
echo ""
echo "================================"
echo -e "Results: ${GREEN}$TESTS_PASSED passed${NC}, ${RED}$TESTS_FAILED failed${NC} (out of $TESTS_RUN)"
echo "================================"

if [[ $TESTS_FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
