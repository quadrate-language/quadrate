#!/bin/bash
# Automated SSE integration tests
# Runs the SSE test server and verifies output

# Continue on error to run all tests
set +e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT=19877
PASS=0
FAIL=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

# Build the test server
echo "Building SSE test server..."
cd "$SCRIPT_DIR"
quad build sse_integration_test.qd -o sse_test_server 2>/dev/null || {
    echo -e "${RED}Failed to build test server${NC}"
    exit 1
}

# Start server in background
./sse_test_server > /dev/null 2>&1 &
SERVER_PID=$!
sleep 0.5

# Cleanup on exit
cleanup() {
    kill $SERVER_PID 2>/dev/null || true
    rm -f sse_test_server
}
trap cleanup EXIT

# Test function using pattern matching
test_sse() {
    local name="$1"
    local endpoint="$2"
    shift 2
    local patterns=("$@")

    local output
    output=$(timeout 2 curl -N -s "http://localhost:$PORT$endpoint" 2>/dev/null || true)

    local all_found=1
    for pattern in "${patterns[@]}"; do
        if ! echo "$output" | grep -qF "$pattern"; then
            all_found=0
            break
        fi
    done

    if [ "$all_found" -eq 1 ]; then
        echo -e "${GREEN}✓${NC} $name"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}✗${NC} $name"
        echo "  Output:"
        echo "$output" | sed 's/^/    /'
        echo "  Expected patterns: ${patterns[*]}"
        FAIL=$((FAIL + 1))
    fi
}

# Test for exact line count
test_sse_lines() {
    local name="$1"
    local endpoint="$2"
    local expected_lines="$3"

    local output
    output=$(timeout 2 curl -N -s "http://localhost:$PORT$endpoint" 2>/dev/null || true)
    local actual_lines
    actual_lines=$(echo "$output" | grep -c "^data:" || true)

    if [ "$actual_lines" -eq "$expected_lines" ]; then
        echo -e "${GREEN}✓${NC} $name ($actual_lines data lines)"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}✗${NC} $name (expected $expected_lines data lines, got $actual_lines)"
        FAIL=$((FAIL + 1))
    fi
}

# Test for empty response
test_sse_empty() {
    local name="$1"
    local endpoint="$2"

    local output
    output=$(timeout 2 curl -N -s "http://localhost:$PORT$endpoint" 2>/dev/null || true)

    if [ -z "$output" ]; then
        echo -e "${GREEN}✓${NC} $name"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}✗${NC} $name (expected empty, got: '$output')"
        FAIL=$((FAIL + 1))
    fi
}

echo ""
echo "Running SSE tests..."
echo "===================="
echo ""

echo "Basic functionality:"
test_sse "Single data event" "/test/single" "data: hello world"
test_sse "Multiple data events" "/test/multiple" "data: first" "data: second" "data: third"
test_sse_lines "Multiple events count" "/test/multiple" 3

echo ""
echo "Named events:"
test_sse "Named events - greeting" "/test/named" "event: greeting" "data: hello"
test_sse "Named events - farewell" "/test/named" "event: farewell" "data: goodbye"

echo ""
echo "Mixed events:"
test_sse "Mixed - data only" "/test/mixed" "data: data only"
test_sse "Mixed - named event" "/test/mixed" "event: status" "data: ok"
test_sse "Mixed - done event" "/test/mixed" "event: done" "data: true"

echo ""
echo "Loop and streaming:"
test_sse "Loop events" "/test/loop" "data: 1" "data: 2" "data: 3" "data: 4" "data: 5"
test_sse_lines "Loop count" "/test/loop" 5
test_sse "Streaming with delay" "/test/streaming" "data: 1" "event: complete"

echo ""
echo "Edge cases:"
test_sse_empty "Empty stream" "/test/empty"
test_sse "Long payload" "/test/long" "data:" "longer message"
test_sse "Special chars - colon" "/test/special" "data: hello:world"
test_sse "Special chars - equals" "/test/special" "data: key=value"

echo ""
echo "Common patterns:"
test_sse "JSON-like data" "/test/json" "data: {'type':'init'" "event: complete"
test_sse "Counter pattern" "/test/counter" "event: count" "event: done"
test_sse "Progress pattern" "/test/progress" "event: progress" "data: 0" "data: 100" "event: complete"

# Summary
echo ""
echo "===================="
echo -e "Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC}"

if [ $FAIL -gt 0 ]; then
    exit 1
fi

echo ""
echo "All SSE tests passed!"
