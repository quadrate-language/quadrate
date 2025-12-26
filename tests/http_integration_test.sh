#!/bin/bash
# Integration test for HTTP server

set -e

QUADC="${QUADC:-build/debug/cmd/quadc/quadc}"
export QUADRATE_LIBDIR="${QUADRATE_LIBDIR:-dist/lib}"
export QUADRATE_ROOT="${QUADRATE_ROOT:-dist/share/quadrate}"

PORT=8765
TIMEOUT=5
PASSED=0
FAILED=0

# Compile the server
echo "Compiling test server..."
$QUADC tests/qd/http/http_server_integration.qd -o /tmp/http_test_server

# Start server in background
echo "Starting server on port $PORT..."
/tmp/http_test_server &
SERVER_PID=$!

# Wait for server to start
sleep 0.5

# Cleanup function
cleanup() {
    echo "Stopping server..."
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    rm -f /tmp/http_test_server
}
trap cleanup EXIT

# Test function
test_request() {
    local name="$1"
    local url="$2"
    local expected="$3"
    local extra_args="${4:-}"

    echo -n "  Testing $name... "

    local response
    response=$(curl -s --max-time $TIMEOUT $extra_args "http://localhost:$PORT$url" 2>&1) || {
        echo "FAILED (curl error)"
        FAILED=$((FAILED + 1))
        return
    }

    if [ "$response" = "$expected" ]; then
        echo "PASS"
        PASSED=$((PASSED + 1))
    else
        echo "FAILED"
        echo "    Expected: $expected"
        echo "    Got: $response"
        FAILED=$((FAILED + 1))
    fi
}

test_content_type() {
    local name="$1"
    local url="$2"
    local expected_type="$3"

    echo -n "  Testing $name content-type... "

    local content_type
    # Use -i to get headers with GET request (not HEAD which is -I)
    content_type=$(curl -s -i --max-time $TIMEOUT "http://localhost:$PORT$url" 2>&1 | grep -i "Content-Type:" | cut -d' ' -f2 | tr -d '\r') || {
        echo "FAILED (curl error)"
        FAILED=$((FAILED + 1))
        return
    }

    if echo "$content_type" | grep -q "$expected_type"; then
        echo "PASS"
        PASSED=$((PASSED + 1))
    else
        echo "FAILED"
        echo "    Expected: $expected_type"
        echo "    Got: $content_type"
        FAILED=$((FAILED + 1))
    fi
}

echo ""
echo "Running HTTP server tests..."
echo ""

# Basic tests
test_request "GET /" "/" "Hello, World!"
test_request "GET /json" "/json" "{}"
test_request "GET /html" "/html" "<h1>Hello</h1>"

# Path parameters
test_request "GET /users/123" "/users/123" "123"
test_request "GET /users/abc" "/users/abc" "abc"

# Query parameters
test_request "GET /search?q=test" "/search?q=test" "test"
test_request "GET /search?q=hello+world" "/search?q=hello+world" "hello+world"

# Header echo - use array to handle header correctly
echo -n "  Testing GET /echo with header... "
header_response=$(curl -s --max-time $TIMEOUT -H "X-Test: test-value" "http://localhost:$PORT/echo" 2>&1) || {
    echo "FAILED (curl error)"
    FAILED=$((FAILED + 1))
}
if [ "$header_response" = "test-value" ]; then
    echo "PASS"
    PASSED=$((PASSED + 1))
else
    echo "FAILED"
    echo "    Expected: test-value"
    echo "    Got: $header_response"
    FAILED=$((FAILED + 1))
fi

# Content-Type tests
test_content_type "JSON" "/json" "application/json"
test_content_type "HTML" "/html" "text/html"
test_content_type "plain text" "/" "text/plain"

# 404 test
echo -n "  Testing 404... "
response_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time $TIMEOUT "http://localhost:$PORT/nonexistent" 2>&1)
if [ "$response_code" = "404" ]; then
    echo "PASS"
    PASSED=$((PASSED + 1))
else
    echo "FAILED (got $response_code)"
    FAILED=$((FAILED + 1))
fi

echo ""
echo "=========================================="
echo "Results: $PASSED passed, $FAILED failed"
echo "=========================================="

if [ $FAILED -gt 0 ]; then
    exit 1
fi

echo "All tests passed!"
