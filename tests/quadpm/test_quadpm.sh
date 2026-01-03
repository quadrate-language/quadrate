#!/bin/bash

# Test suite for quadpm

set -u

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Get quadpm binary path (convert to absolute path)
QUADPM="${QUADPM:-build/debug/cmd/quadpm/quadpm}"
# Convert to absolute path if relative
if [[ "$QUADPM" != /* ]]; then
    QUADPM="$(pwd)/$QUADPM"
fi

# Temporary directory for tests
TEST_CACHE_DIR="/tmp/quadpm_test_$$"

# Cleanup function
cleanup() {
    rm -rf "$TEST_CACHE_DIR"
}

trap cleanup EXIT

# Test helper functions
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

skip() {
    echo -e "${YELLOW}⊘${NC} $1"
}

# Check if quadpm binary exists
if [ ! -x "$QUADPM" ]; then
    echo -e "${RED}Error: quadpm binary not found at $QUADPM${NC}"
    exit 1
fi

echo "Testing quadpm..."
echo "Binary: $QUADPM"
echo ""

# Test 1: Help output
echo "Test 1: Help output"
if output=$("$QUADPM" --help 2>&1); then
    if echo "$output" | grep -q "quadpm - Quadrate module manager"; then
        pass "Help message displays correctly"
    else
        fail "Help message missing expected content" "$output"
    fi
else
    fail "Help command failed" ""
fi

# Test 2: Version output
echo ""
echo "Test 2: Version output"
if output=$("$QUADPM" --version 2>&1); then
    if echo "$output" | grep -q "quadpm 2.0.0-alpha"; then
        pass "Version displays correctly"
    else
        fail "Version output incorrect" "$output"
    fi
else
    fail "Version command failed" ""
fi

# Test 3: No arguments
echo ""
echo "Test 3: No arguments (should show help)"
if output=$("$QUADPM" 2>&1); then
    # Should exit with error and show usage
    if echo "$output" | grep -q "Usage:"; then
        pass "Shows usage when no arguments provided"
    else
        fail "Should show usage" "$output"
    fi
else
    # Expected to fail, but should show help
    pass "Exits with error when no arguments"
fi

# Test 4: Unknown command
echo ""
echo "Test 4: Unknown command"
if output=$("$QUADPM" foobar 2>&1); then
    fail "Should reject unknown command" "$output"
else
    if echo "$output" | grep -q "Unknown command"; then
        pass "Rejects unknown command with error"
    else
        fail "Error message unclear" "$output"
    fi
fi

# Test 5: get without URL
echo ""
echo "Test 5: get command without URL"
if output=$("$QUADPM" get 2>&1); then
    fail "Should require URL argument" "$output"
else
    if echo "$output" | grep -q "requires a Git URL"; then
        pass "Rejects get without URL"
    else
        fail "Error message unclear" "$output"
    fi
fi

# Test 6: list with empty cache
echo ""
echo "Test 6: list with empty cache"
if output=$(QUADRATE_PATH="$TEST_CACHE_DIR" "$QUADPM" list 2>&1); then
    if echo "$output" | grep -q "No modules installed"; then
        pass "Lists empty cache correctly"
    else
        fail "Unexpected output for empty cache" "$output"
    fi
else
    fail "List command failed" ""
fi

# Test 7: get from invalid URL (local non-existent path, no network)
echo ""
echo "Test 7: get from non-existent URL"
if output=$(QUADRATE_PATH="$TEST_CACHE_DIR" "$QUADPM" get file:///nonexistent/path/to/repo@v1.0.0 2>&1); then
    fail "Should fail for invalid URL" "$output"
else
    if echo "$output" | grep -q "Failed to clone"; then
        pass "Rejects invalid Git URL"
    else
        fail "Error message unclear" "$output"
    fi
fi

# Test 8: get with invalid ref format
echo ""
echo "Test 8: get from local repo with invalid ref"
# Create a test repo
TEST_REPO="$TEST_CACHE_DIR/test-repo"
mkdir -p "$TEST_REPO"
cd "$TEST_REPO"
git init -q
git config user.name "Test User"
git config user.email "test@example.com"
git config commit.gpgSign false
git config tag.gpgSign false
echo "fn test() { }" > module.qd
git add module.qd
git commit -q -m "Initial"

# Try to get non-existent tag
if output=$(QUADRATE_PATH="$TEST_CACHE_DIR/cache" "$QUADPM" get "$TEST_REPO@nonexistent-tag" 2>&1); then
    fail "Should fail for non-existent ref" "$output"
else
    if echo "$output" | grep -q "Failed to clone"; then
        pass "Rejects non-existent Git ref"
    else
        fail "Error message unclear" "$output"
    fi
fi

cd - > /dev/null

# Test 9: Successful installation
echo ""
echo "Test 9: Successful package installation"
cd "$TEST_REPO"
git tag -a v1.0.0 -m "Version 1.0.0"
cd - > /dev/null

if output=$(QUADRATE_PATH="$TEST_CACHE_DIR/cache" "$QUADPM" get "$TEST_REPO@v1.0.0" 2>&1); then
    if echo "$output" | grep -q "✓ Installed to"; then
        if echo "$output" | grep -q "✓ Found module.qd"; then
            # Verify package was actually created (uses Go-style path: local/path/to/repo@version)
            if find "$TEST_CACHE_DIR/cache" -type d -name "test-repo@v1.0.0" | grep -q .; then
                pass "Installs package successfully"
            else
                fail "Package directory not created" "$output"
            fi
        else
            fail "Module.qd not found after install" "$output"
        fi
    else
        fail "Installation output incorrect" "$output"
    fi
else
    fail "Installation failed" "$output"
fi

# Test 10: Duplicate installation
echo ""
echo "Test 10: Duplicate installation (should skip)"
if output=$(QUADRATE_PATH="$TEST_CACHE_DIR/cache" "$QUADPM" get "$TEST_REPO@v1.0.0" 2>&1); then
    if echo "$output" | grep -q "Module already exists"; then
        pass "Detects duplicate installation"
    else
        fail "Should detect existing package" "$output"
    fi
else
    fail "Duplicate check failed" "$output"
fi

# Test 11: list with installed package
echo ""
echo "Test 11: list with installed packages"
if output=$(QUADRATE_PATH="$TEST_CACHE_DIR/cache" "$QUADPM" list 2>&1); then
    if echo "$output" | grep -q "test-repo"; then
        if echo "$output" | grep -q "v1.0.0"; then
            pass "Lists installed packages"
        else
            fail "Package version not shown" "$output"
        fi
    else
        fail "Package not listed" "$output"
    fi
else
    fail "List command failed" ""
fi

# Test 12: Package with C sources
echo ""
echo "Test 12: Package with C sources"
TEST_C_REPO="$TEST_CACHE_DIR/test-c-repo"
mkdir -p "$TEST_C_REPO/src"
cd "$TEST_C_REPO"
git init -q
git config user.name "Test User"
git config user.email "test@example.com"
git config commit.gpgSign false
git config tag.gpgSign false
echo "fn test() { }" > module.qd
cat > src/test.c << 'EOF'
int add(int a, int b) {
    return a + b;
}
EOF
git add .
git commit -q -m "Initial"
git tag -a v1.0.0 -m "Version 1.0.0"
cd - > /dev/null

if output=$(QUADRATE_PATH="$TEST_CACHE_DIR/cache" "$QUADPM" get "$TEST_C_REPO@v1.0.0" 2>&1); then
    if echo "$output" | grep -q "Found src/ directory"; then
        if echo "$output" | grep -q "✓ Built"; then
            # Verify library was created (uses Go-style path: local/path/to/repo@version/lib/)
            # Library name has 'qd' prefix: libqd<module>_static.a
            if find "$TEST_CACHE_DIR/cache" -name "libqdtest-c-repo_static.a" | grep -q .; then
                pass "Compiles C sources successfully"
            else
                fail "Library file not created" "$output"
            fi
        else
            fail "C compilation output missing" "$output"
        fi
    else
        fail "src/ directory not detected" "$output"
    fi
else
    fail "Installation with C sources failed" "$output"
fi

# Test 13: Package with invalid C code
echo ""
echo "Test 13: Package with invalid C code"
TEST_BAD_C_REPO="$TEST_CACHE_DIR/test-bad-c-repo"
mkdir -p "$TEST_BAD_C_REPO/src"
cd "$TEST_BAD_C_REPO"
git init -q
git config user.name "Test User"
git config user.email "test@example.com"
git config commit.gpgSign false
git config tag.gpgSign false
echo "fn test() { }" > module.qd
cat > src/bad.c << 'EOF'
#include <nonexistent.h>
this is not valid C code
EOF
git add .
git commit -q -m "Initial"
git tag -a v1.0.0 -m "Version 1.0.0"
cd - > /dev/null

if output=$(QUADRATE_PATH="$TEST_CACHE_DIR/cache" "$QUADPM" get "$TEST_BAD_C_REPO@v1.0.0" 2>&1); then
    if echo "$output" | grep -q "✗ Failed to compile"; then
        # Should install but fail C compilation
        pass "Handles C compilation errors gracefully"
    else
        # Package installed, but should have shown compilation warning
        skip "C compilation error not clearly reported"
    fi
else
    fail "Should install package even if C compilation fails" "$output"
fi

# Test 14: XDG_DATA_HOME support
echo ""
echo "Test 14: XDG_DATA_HOME support"
XDG_DIR="$TEST_CACHE_DIR/xdg-test"
if output=$(XDG_DATA_HOME="$XDG_DIR" "$QUADPM" list 2>&1); then
    if echo "$output" | grep -q "$XDG_DIR/quadrate/modules"; then
        pass "Respects XDG_DATA_HOME"
    else
        fail "XDG_DATA_HOME not used" "$output"
    fi
else
    fail "List with XDG_DATA_HOME failed" ""
fi

# Test 15: QUADRATE_PATH override
echo ""
echo "Test 15: QUADRATE_PATH override"
CUSTOM_CACHE="$TEST_CACHE_DIR/custom-cache"
if output=$(QUADRATE_PATH="$CUSTOM_CACHE" "$QUADPM" list 2>&1); then
    if echo "$output" | grep -q "$CUSTOM_CACHE"; then
        pass "Respects QUADRATE_PATH"
    else
        fail "QUADRATE_PATH not used" "$output"
    fi
else
    fail "List with QUADRATE_PATH failed" ""
fi

# ============================================
# Lockfile Tests
# ============================================

# Test 16: install --frozen without lockfile
echo ""
echo "Test 16: install --frozen without lockfile"
LOCKFILE_TEST_DIR="$TEST_CACHE_DIR/lockfile-test"
mkdir -p "$LOCKFILE_TEST_DIR"
cat > "$LOCKFILE_TEST_DIR/qd.json" << 'EOF'
{
  "name": "lockfile-test",
  "dependencies": {
    "testmod": "./testmod"
  }
}
EOF
mkdir -p "$LOCKFILE_TEST_DIR/testmod"
echo "fn test() { 1 }" > "$LOCKFILE_TEST_DIR/testmod/module.qd"

cd "$LOCKFILE_TEST_DIR"
if output=$("$QUADPM" install --frozen 2>&1); then
    fail "Should fail without lockfile" "$output"
else
    if echo "$output" | grep -q "requires qd.lock"; then
        pass "install --frozen fails without lockfile"
    else
        fail "Error message unclear" "$output"
    fi
fi
cd - > /dev/null

# Test 17: install creates lockfile
echo ""
echo "Test 17: install creates lockfile"
cd "$LOCKFILE_TEST_DIR"
if output=$("$QUADPM" install 2>&1); then
    if [ -f "qd.lock" ]; then
        if grep -q '"version": 1' qd.lock; then
            if grep -q '"testmod"' qd.lock; then
                pass "install creates valid lockfile"
            else
                fail "Lockfile missing dependency" "$(cat qd.lock)"
            fi
        else
            fail "Lockfile missing version" "$(cat qd.lock)"
        fi
    else
        fail "Lockfile not created" "$output"
    fi
else
    fail "Install command failed" "$output"
fi
cd - > /dev/null

# Test 18: install --frozen with valid lockfile
echo ""
echo "Test 18: install --frozen with valid lockfile"
cd "$LOCKFILE_TEST_DIR"
if output=$("$QUADPM" install --frozen 2>&1); then
    if echo "$output" | grep -q "Installing from lockfile"; then
        pass "install --frozen works with valid lockfile"
    else
        fail "Should indicate lockfile usage" "$output"
    fi
else
    fail "install --frozen failed with valid lockfile" "$output"
fi
cd - > /dev/null

# Test 19: install --frozen with outdated lockfile (missing dep)
echo ""
echo "Test 19: install --frozen with outdated lockfile"
cd "$LOCKFILE_TEST_DIR"
# Add new dependency to qd.json but not to lockfile
mkdir -p "$LOCKFILE_TEST_DIR/newmod"
echo "fn new() { 2 }" > "$LOCKFILE_TEST_DIR/newmod/module.qd"
cat > "$LOCKFILE_TEST_DIR/qd.json" << 'EOF'
{
  "name": "lockfile-test",
  "dependencies": {
    "testmod": "./testmod",
    "newmod": "./newmod"
  }
}
EOF
if output=$("$QUADPM" install --frozen 2>&1); then
    fail "Should fail with outdated lockfile" "$output"
else
    if echo "$output" | grep -q "not in lockfile"; then
        pass "install --frozen detects outdated lockfile"
    else
        fail "Error message unclear" "$output"
    fi
fi
cd - > /dev/null

# Test 20: lock command generates lockfile
echo ""
echo "Test 20: lock command generates lockfile"
cd "$LOCKFILE_TEST_DIR"
rm -f qd.lock
if output=$("$QUADPM" lock 2>&1); then
    if [ -f "qd.lock" ]; then
        if grep -q '"newmod"' qd.lock && grep -q '"testmod"' qd.lock; then
            pass "lock command generates complete lockfile"
        else
            fail "Lockfile incomplete" "$(cat qd.lock)"
        fi
    else
        fail "Lockfile not created" "$output"
    fi
else
    fail "lock command failed" "$output"
fi
cd - > /dev/null

# Test 21: lock command with missing dependencies
echo ""
echo "Test 21: lock command with missing dependencies"
LOCK_MISSING_DIR="$TEST_CACHE_DIR/lock-missing-test"
mkdir -p "$LOCK_MISSING_DIR"
cat > "$LOCK_MISSING_DIR/qd.json" << 'EOF'
{
  "name": "lock-missing-test",
  "dependencies": {
    "nonexistent": "./nonexistent"
  }
}
EOF
cd "$LOCK_MISSING_DIR"
if output=$("$QUADPM" lock 2>&1); then
    fail "Should report missing dependencies" "$output"
else
    if echo "$output" | grep -q "not found\|not installed"; then
        pass "lock command reports missing dependencies"
    else
        fail "Error message unclear" "$output"
    fi
fi
cd - > /dev/null

# Test 22: lockfile with git dependency (using test repo)
echo ""
echo "Test 22: lockfile with git dependency"
LOCK_GIT_DIR="$TEST_CACHE_DIR/lock-git-test"
mkdir -p "$LOCK_GIT_DIR"
# Use file:// protocol to ensure it's treated as a git URL, not a local path
cat > "$LOCK_GIT_DIR/qd.json" << EOF
{
  "name": "lock-git-test",
  "dependencies": {
    "test-repo": "file://$TEST_REPO@v1.0.0"
  }
}
EOF
cd "$LOCK_GIT_DIR"
if output=$(QUADRATE_PATH="$TEST_CACHE_DIR/cache" "$QUADPM" install 2>&1); then
    if [ -f "qd.lock" ]; then
        if grep -q '"resolved":' qd.lock; then
            pass "lockfile records git commit hash"
        else
            fail "Lockfile missing resolved commit" "$(cat qd.lock)"
        fi
    else
        fail "Lockfile not created for git dep" "$output"
    fi
else
    fail "Install with git dep failed" "$output"
fi
cd - > /dev/null

# Test 23: Transitive dependencies
echo ""
echo "Test 23: Transitive dependencies"
TRANSITIVE_DIR="$TEST_CACHE_DIR/transitive-test"
mkdir -p "$TRANSITIVE_DIR/module_c" "$TRANSITIVE_DIR/module_b" "$TRANSITIVE_DIR/module_a" "$TRANSITIVE_DIR/main"

# Module C (leaf)
echo "fn c_func() { 100 }" > "$TRANSITIVE_DIR/module_c/module.qd"

# Module B depends on C
echo "fn b_func() { 200 }" > "$TRANSITIVE_DIR/module_b/module.qd"
cat > "$TRANSITIVE_DIR/module_b/qd.json" << 'EOF'
{
  "name": "module_b",
  "dependencies": {
    "module_c": "../module_c"
  }
}
EOF

# Module A depends on B
echo "fn a_func() { 300 }" > "$TRANSITIVE_DIR/module_a/module.qd"
cat > "$TRANSITIVE_DIR/module_a/qd.json" << 'EOF'
{
  "name": "module_a",
  "dependencies": {
    "module_b": "../module_b"
  }
}
EOF

# Main depends on A
cat > "$TRANSITIVE_DIR/main/qd.json" << 'EOF'
{
  "name": "main",
  "dependencies": {
    "module_a": "../module_a"
  }
}
EOF

cd "$TRANSITIVE_DIR/main"
if output=$("$QUADPM" install 2>&1); then
    # Check that all 3 modules were installed (A, B, C)
    if echo "$output" | grep -q "3 dependencies installed"; then
        if grep -q '"module_c"' qd.lock; then
            pass "Transitive dependencies resolved correctly"
        else
            fail "Transitive dep (module_c) not in lockfile" "$(cat qd.lock)"
        fi
    else
        fail "Should install 3 dependencies (1 direct + 2 transitive)" "$output"
    fi
else
    fail "Install with transitive deps failed" "$output"
fi
cd - > /dev/null

# Test 24: Diamond dependencies (A depends on B and C, both depend on D)
echo ""
echo "Test 24: Diamond dependencies"
DIAMOND_DIR="$TEST_CACHE_DIR/diamond-test"
mkdir -p "$DIAMOND_DIR/mod_d" "$DIAMOND_DIR/mod_c" "$DIAMOND_DIR/mod_b" "$DIAMOND_DIR/mod_a" "$DIAMOND_DIR/main"

# Module D (leaf - shared dependency)
echo "fn d_func() { 400 }" > "$DIAMOND_DIR/mod_d/module.qd"

# Module C depends on D
echo "fn c_func() { 300 }" > "$DIAMOND_DIR/mod_c/module.qd"
cat > "$DIAMOND_DIR/mod_c/qd.json" << 'EOF'
{
  "name": "mod_c",
  "dependencies": {
    "mod_d": "../mod_d"
  }
}
EOF

# Module B depends on D
echo "fn b_func() { 200 }" > "$DIAMOND_DIR/mod_b/module.qd"
cat > "$DIAMOND_DIR/mod_b/qd.json" << 'EOF'
{
  "name": "mod_b",
  "dependencies": {
    "mod_d": "../mod_d"
  }
}
EOF

# Module A depends on B and C
echo "fn a_func() { 100 }" > "$DIAMOND_DIR/mod_a/module.qd"
cat > "$DIAMOND_DIR/mod_a/qd.json" << 'EOF'
{
  "name": "mod_a",
  "dependencies": {
    "mod_b": "../mod_b",
    "mod_c": "../mod_c"
  }
}
EOF

# Main depends on A
cat > "$DIAMOND_DIR/main/qd.json" << 'EOF'
{
  "name": "main",
  "dependencies": {
    "mod_a": "../mod_a"
  }
}
EOF

cd "$DIAMOND_DIR/main"
if output=$("$QUADPM" install 2>&1); then
    # Should install 4 unique deps: A, B, C, D (D only once despite diamond)
    if echo "$output" | grep -q "4 dependencies installed"; then
        # Check D appears exactly once in lockfile
        d_count=$(grep -c '"mod_d"' qd.lock)
        if [ "$d_count" -eq 1 ]; then
            pass "Diamond dependencies deduplicated correctly"
        else
            fail "Module D should appear exactly once in lockfile" "count=$d_count"
        fi
    else
        fail "Should install 4 dependencies" "$output"
    fi
else
    fail "Install with diamond deps failed" "$output"
fi
cd - > /dev/null

# Test 25: Deep dependency chain (5 levels)
echo ""
echo "Test 25: Deep dependency chain (5 levels)"
DEEP_DIR="$TEST_CACHE_DIR/deep-test"
mkdir -p "$DEEP_DIR/level5" "$DEEP_DIR/level4" "$DEEP_DIR/level3" "$DEEP_DIR/level2" "$DEEP_DIR/level1" "$DEEP_DIR/main"

# Level 5 (deepest - no deps)
echo "fn level5() { 5 }" > "$DEEP_DIR/level5/module.qd"

# Level 4 depends on 5
echo "fn level4() { 4 }" > "$DEEP_DIR/level4/module.qd"
cat > "$DEEP_DIR/level4/qd.json" << 'EOF'
{
  "name": "level4",
  "dependencies": { "level5": "../level5" }
}
EOF

# Level 3 depends on 4
echo "fn level3() { 3 }" > "$DEEP_DIR/level3/module.qd"
cat > "$DEEP_DIR/level3/qd.json" << 'EOF'
{
  "name": "level3",
  "dependencies": { "level4": "../level4" }
}
EOF

# Level 2 depends on 3
echo "fn level2() { 2 }" > "$DEEP_DIR/level2/module.qd"
cat > "$DEEP_DIR/level2/qd.json" << 'EOF'
{
  "name": "level2",
  "dependencies": { "level3": "../level3" }
}
EOF

# Level 1 depends on 2
echo "fn level1() { 1 }" > "$DEEP_DIR/level1/module.qd"
cat > "$DEEP_DIR/level1/qd.json" << 'EOF'
{
  "name": "level1",
  "dependencies": { "level2": "../level2" }
}
EOF

# Main depends on level1
cat > "$DEEP_DIR/main/qd.json" << 'EOF'
{
  "name": "main",
  "dependencies": { "level1": "../level1" }
}
EOF

cd "$DEEP_DIR/main"
if output=$("$QUADPM" install 2>&1); then
    if echo "$output" | grep -q "5 dependencies installed"; then
        # Verify deepest level is in lockfile
        if grep -q '"level5"' qd.lock; then
            pass "Deep dependency chain resolved (5 levels)"
        else
            fail "Deepest dependency not in lockfile" "$(cat qd.lock)"
        fi
    else
        fail "Should install 5 dependencies" "$output"
    fi
else
    fail "Install with deep deps failed" "$output"
fi
cd - > /dev/null

# Test 26: Multiple direct deps with shared transitive
echo ""
echo "Test 26: Multiple direct deps with shared transitive"
SHARED_DIR="$TEST_CACHE_DIR/shared-test"
mkdir -p "$SHARED_DIR/shared" "$SHARED_DIR/dep_x" "$SHARED_DIR/dep_y" "$SHARED_DIR/main"

# Shared module (used by both X and Y)
echo "fn shared_func() { 999 }" > "$SHARED_DIR/shared/module.qd"

# Dep X depends on shared
echo "fn x_func() { 1 }" > "$SHARED_DIR/dep_x/module.qd"
cat > "$SHARED_DIR/dep_x/qd.json" << 'EOF'
{
  "name": "dep_x",
  "dependencies": { "shared": "../shared" }
}
EOF

# Dep Y depends on shared
echo "fn y_func() { 2 }" > "$SHARED_DIR/dep_y/module.qd"
cat > "$SHARED_DIR/dep_y/qd.json" << 'EOF'
{
  "name": "dep_y",
  "dependencies": { "shared": "../shared" }
}
EOF

# Main depends on both X and Y directly
cat > "$SHARED_DIR/main/qd.json" << 'EOF'
{
  "name": "main",
  "dependencies": {
    "dep_x": "../dep_x",
    "dep_y": "../dep_y"
  }
}
EOF

cd "$SHARED_DIR/main"
if output=$("$QUADPM" install 2>&1); then
    # Should install 3 deps: X, Y, shared (shared only once)
    if echo "$output" | grep -q "3 dependencies installed"; then
        shared_count=$(grep -c '"shared"' qd.lock)
        if [ "$shared_count" -eq 1 ]; then
            pass "Shared transitive dep installed once"
        else
            fail "Shared dep should appear once" "count=$shared_count"
        fi
    else
        fail "Should install 3 dependencies" "$output"
    fi
else
    fail "Install with shared deps failed" "$output"
fi
cd - > /dev/null

# Test 27: Re-install with existing transitive deps
echo ""
echo "Test 27: Re-install with existing transitive deps"
cd "$SHARED_DIR/main"
rm -f qd.lock
if output=$("$QUADPM" install 2>&1); then
    # Run install again - should use lockfile and succeed
    if output2=$("$QUADPM" install 2>&1); then
        # For local deps, it uses lockfile on second run
        if echo "$output2" | grep -q "Installing from lockfile\|3 dependencies installed"; then
            pass "Re-install handles existing transitive deps"
        else
            fail "Should complete successfully" "$output2"
        fi
    else
        fail "Re-install failed" "$output2"
    fi
else
    fail "Initial install failed" "$output"
fi
cd - > /dev/null

# Test 28: Transitive dep with empty dependencies
echo ""
echo "Test 28: Transitive dep with qd.json but no dependencies"
EMPTY_DEPS_DIR="$TEST_CACHE_DIR/empty-deps-test"
mkdir -p "$EMPTY_DEPS_DIR/leaf" "$EMPTY_DEPS_DIR/middle" "$EMPTY_DEPS_DIR/main"

# Leaf has qd.json but empty dependencies
echo "fn leaf_func() { 1 }" > "$EMPTY_DEPS_DIR/leaf/module.qd"
cat > "$EMPTY_DEPS_DIR/leaf/qd.json" << 'EOF'
{
  "name": "leaf",
  "dependencies": {}
}
EOF

# Middle depends on leaf
echo "fn middle_func() { 2 }" > "$EMPTY_DEPS_DIR/middle/module.qd"
cat > "$EMPTY_DEPS_DIR/middle/qd.json" << 'EOF'
{
  "name": "middle",
  "dependencies": { "leaf": "../leaf" }
}
EOF

# Main depends on middle
cat > "$EMPTY_DEPS_DIR/main/qd.json" << 'EOF'
{
  "name": "main",
  "dependencies": { "middle": "../middle" }
}
EOF

cd "$EMPTY_DEPS_DIR/main"
if output=$("$QUADPM" install 2>&1); then
    if echo "$output" | grep -q "2 dependencies installed"; then
        pass "Handles qd.json with empty dependencies"
    else
        fail "Should install 2 dependencies" "$output"
    fi
else
    fail "Install failed" "$output"
fi
cd - > /dev/null

# Test 29: Transitive dep without qd.json (pure Quadrate module)
echo ""
echo "Test 29: Transitive dep without qd.json"
PURE_DIR="$TEST_CACHE_DIR/pure-test"
mkdir -p "$PURE_DIR/pure_leaf" "$PURE_DIR/wrapper" "$PURE_DIR/main"

# Pure leaf - only module.qd, no qd.json
echo "fn pure_func() { 42 }" > "$PURE_DIR/pure_leaf/module.qd"

# Wrapper depends on pure_leaf
echo "fn wrapper_func() { 1 }" > "$PURE_DIR/wrapper/module.qd"
cat > "$PURE_DIR/wrapper/qd.json" << 'EOF'
{
  "name": "wrapper",
  "dependencies": { "pure_leaf": "../pure_leaf" }
}
EOF

# Main depends on wrapper
cat > "$PURE_DIR/main/qd.json" << 'EOF'
{
  "name": "main",
  "dependencies": { "wrapper": "../wrapper" }
}
EOF

cd "$PURE_DIR/main"
if output=$("$QUADPM" install 2>&1); then
    if echo "$output" | grep -q "2 dependencies installed"; then
        if grep -q '"pure_leaf"' qd.lock; then
            pass "Handles transitive dep without qd.json"
        else
            fail "Pure leaf not in lockfile" "$(cat qd.lock)"
        fi
    else
        fail "Should install 2 dependencies" "$output"
    fi
else
    fail "Install failed" "$output"
fi
cd - > /dev/null

# Test 30: Frozen install with transitive deps in lockfile
echo ""
echo "Test 30: Frozen install with transitive deps"
cd "$PURE_DIR/main"
# First create lockfile
"$QUADPM" install > /dev/null 2>&1
# Now try frozen install
if output=$("$QUADPM" install --frozen 2>&1); then
    if echo "$output" | grep -q "Installing from lockfile"; then
        pass "Frozen install works with transitive deps"
    else
        fail "Should use lockfile" "$output"
    fi
else
    fail "Frozen install failed" "$output"
fi
cd - > /dev/null

# Test 31: Namespace registration
echo ""
echo "Test 31: Namespace registration"
NAMESPACE_DIR="$TEST_CACHE_DIR/namespace-test"
mkdir -p "$NAMESPACE_DIR/mylib"

# Create a module with custom namespace
echo "fn lib_func() { 1 }" > "$NAMESPACE_DIR/mylib/module.qd"
cat > "$NAMESPACE_DIR/mylib/qd.json" << 'EOF'
{
  "name": "mylib",
  "namespace": "ml"
}
EOF
cd "$NAMESPACE_DIR/mylib"
git init -q
git config user.name "Test User"
git config user.email "test@example.com"
git config commit.gpgSign false
git config tag.gpgSign false
git add .
git commit -q -m "Initial"
git tag -a v1.0.0 -m "v1.0.0"
cd - > /dev/null

if output=$(QUADRATE_PATH="$TEST_CACHE_DIR/cache" "$QUADPM" get "$NAMESPACE_DIR/mylib@v1.0.0" 2>&1); then
    if echo "$output" | grep -q "Namespace 'ml' registered"; then
        # Check that symlink was created
        if [ -L "$TEST_CACHE_DIR/cache/_namespaces/ml" ]; then
            pass "Namespace registered with symlink"
        else
            fail "Namespace symlink not created" "$output"
        fi
    else
        fail "Namespace not registered" "$output"
    fi
else
    fail "Installation failed" "$output"
fi

# Test 32: Namespace from module name (no explicit namespace)
echo ""
echo "Test 32: Namespace defaults to module name"
mkdir -p "$NAMESPACE_DIR/anotherlib"
echo "fn another_func() { 2 }" > "$NAMESPACE_DIR/anotherlib/module.qd"
cat > "$NAMESPACE_DIR/anotherlib/qd.json" << 'EOF'
{
  "name": "anotherlib"
}
EOF
cd "$NAMESPACE_DIR/anotherlib"
git init -q
git config user.name "Test User"
git config user.email "test@example.com"
git config commit.gpgSign false
git config tag.gpgSign false
git add .
git commit -q -m "Initial"
git tag -a v1.0.0 -m "v1.0.0"
cd - > /dev/null

if output=$(QUADRATE_PATH="$TEST_CACHE_DIR/cache" "$QUADPM" get "$NAMESPACE_DIR/anotherlib@v1.0.0" 2>&1); then
    if echo "$output" | grep -q "Namespace 'anotherlib' registered"; then
        if [ -L "$TEST_CACHE_DIR/cache/_namespaces/anotherlib" ]; then
            pass "Namespace defaults to module name"
        else
            fail "Namespace symlink not created" "$output"
        fi
    else
        fail "Namespace not registered" "$output"
    fi
else
    fail "Installation failed" "$output"
fi

# Test 33: Namespace conflict warning
echo ""
echo "Test 33: Namespace conflict warning"
mkdir -p "$NAMESPACE_DIR/conflicting"
echo "fn conflict_func() { 3 }" > "$NAMESPACE_DIR/conflicting/module.qd"
# Use same namespace "ml" as Test 31
cat > "$NAMESPACE_DIR/conflicting/qd.json" << 'EOF'
{
  "name": "conflicting",
  "namespace": "ml"
}
EOF
cd "$NAMESPACE_DIR/conflicting"
git init -q
git config user.name "Test User"
git config user.email "test@example.com"
git config commit.gpgSign false
git config tag.gpgSign false
git add .
git commit -q -m "Initial"
git tag -a v1.0.0 -m "v1.0.0"
cd - > /dev/null

if output=$(QUADRATE_PATH="$TEST_CACHE_DIR/cache" "$QUADPM" get "$NAMESPACE_DIR/conflicting@v1.0.0" 2>&1); then
    if echo "$output" | grep -q "namespace 'ml' also claimed by"; then
        if echo "$output" | grep -q "Use full path"; then
            pass "Namespace conflict detected with instructions"
        else
            fail "Missing disambiguation instructions" "$output"
        fi
    else
        fail "Namespace conflict not detected" "$output"
    fi
else
    fail "Installation failed" "$output"
fi

# Test 34: Go-style directory structure
echo ""
echo "Test 34: Go-style directory structure"
# Check that the directory structure uses host/path format
# For git URLs it should be like: git.sr.ht/~user/repo@version
# For local paths it should be like: local/path/to/repo@version
if find "$TEST_CACHE_DIR/cache" -type d -name "mylib@v1.0.0" | grep -q "local/"; then
    pass "Go-style directory structure for local paths"
else
    fail "Wrong directory structure" "$(find "$TEST_CACHE_DIR/cache" -type d -name '*@*')"
fi

# Test 35: List shows namespaces
echo ""
echo "Test 35: List command shows namespaces"
if output=$(QUADRATE_PATH="$TEST_CACHE_DIR/cache" "$QUADPM" list 2>&1); then
    if echo "$output" | grep -q "namespace:"; then
        if echo "$output" | grep -q "Registered namespaces:"; then
            pass "List command shows namespaces"
        else
            fail "Namespace section not shown" "$output"
        fi
    else
        fail "Namespace info not shown in list" "$output"
    fi
else
    fail "List command failed" "$output"
fi

# Print summary
echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo "Tests run: $TESTS_RUN"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $TESTS_FAILED${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi
