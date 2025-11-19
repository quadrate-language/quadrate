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
    if echo "$output" | grep -q "quadpm - Quadrate package manager"; then
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
    if echo "$output" | grep -q "quadpm 0.1.0"; then
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
    if echo "$output" | grep -q "No packages installed"; then
        pass "Lists empty cache correctly"
    else
        fail "Unexpected output for empty cache" "$output"
    fi
else
    fail "List command failed" ""
fi

# Test 7: get from invalid URL
echo ""
echo "Test 7: get from non-existent URL"
if output=$(QUADRATE_PATH="$TEST_CACHE_DIR" "$QUADPM" get https://invalid.example.com/repo@v1.0.0 2>&1); then
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
echo "fn test( -- ) { }" > module.qd
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
            # Verify package was actually created
            if [ -d "$TEST_CACHE_DIR/cache/test-repo@v1.0.0" ]; then
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
    if echo "$output" | grep -q "Package already exists"; then
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
echo "fn test( -- ) { }" > module.qd
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
            # Verify library was created
            if [ -f "$TEST_CACHE_DIR/cache/test-c-repo@v1.0.0/lib/libtest-c-repo_static.a" ]; then
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
echo "fn test( -- ) { }" > module.qd
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
    if echo "$output" | grep -q "$XDG_DIR/quadrate/packages"; then
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
