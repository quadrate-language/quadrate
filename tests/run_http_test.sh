#!/bin/bash

# Test runner for the HTTP server and SSE integration tests.
#
# tests/qd/http/{server_integration,sse_integration}.qd both end in a blocking
# `http::run!`, so the ordinary compile-run-diff harness in run_all.sh cannot
# drive them -- it would hang. They sat with no .out sibling and were silently
# skipped instead. This script compiles each one, starts it, drives the routes
# with curl and diffs the responses.
#
# Usage: run_http_test.sh

set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

WORK_DIR=$(mktemp -d)
SERVER_PID=""

cleanup() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

QUADC="${QUADC:-$PROJECT_ROOT/build/debug/cmd/quadc/quadc}"
if [ ! -x "$QUADC" ]; then
    echo -e "${RED}FAIL${NC}: quadc not found at $QUADC"
    exit 1
fi

if ! command -v curl > /dev/null 2>&1; then
    echo -e "${RED}SKIP${NC}: curl not found, skipping HTTP integration tests"
    exit 0
fi

# The http module ships in dist/share/quadrate after `make`; fall back to the
# in-tree lib/ layout so the script also works from a bare build directory.
if [ -d "$PROJECT_ROOT/dist/share/quadrate/http" ]; then
    export QUADRATE_ROOT="$PROJECT_ROOT/dist/share/quadrate"
elif [ -d "$PROJECT_ROOT/lib/http/qd/http" ]; then
    export QUADRATE_ROOT="$PROJECT_ROOT/lib"
else
    echo -e "${RED}SKIP${NC}: http module not found, skipping HTTP integration tests"
    exit 0
fi

TESTS_RUN=0
TESTS_FAILED=0

# start_server <binary> <readiness-url>
start_server() {
    local binary="$1" ready_url="$2" i
    "$binary" > "$WORK_DIR/server.log" 2>&1 &
    SERVER_PID=$!

    for i in $(seq 1 100); do
        if curl -s -o /dev/null --max-time 2 "$ready_url" 2>/dev/null; then
            return 0
        fi
        if ! kill -0 "$SERVER_PID" 2>/dev/null; then
            echo -e "${RED}FAIL${NC}: server exited during startup"
            cat "$WORK_DIR/server.log"
            exit 1
        fi
        sleep 0.1
    done

    echo -e "${RED}FAIL${NC}: server did not become ready"
    cat "$WORK_DIR/server.log"
    exit 1
}

stop_server() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
        SERVER_PID=""
    fi
}

# check <name> <expected> <curl-args...>
check() {
    local name="$1" expected="$2"
    shift 2
    local actual
    TESTS_RUN=$((TESTS_RUN + 1))
    actual=$(timeout 10 curl -sN --max-time 8 "$@" 2>/dev/null || true)
    if [ "$actual" = "$expected" ]; then
        echo -e "  ${GREEN}✓${NC} $name"
    else
        echo -e "  ${RED}✗${NC} $name"
        echo "      expected: $(printf '%q' "$expected")"
        echo "      actual:   $(printf '%q' "$actual")"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

echo "=== HTTP server integration ==="
"$QUADC" "$PROJECT_ROOT/tests/qd/http/server_integration.qd" -o "$WORK_DIR/server" > "$WORK_DIR/build.log" 2>&1 || {
    echo -e "${RED}FAIL${NC}: could not compile server_integration.qd"
    cat "$WORK_DIR/build.log"
    exit 1
}

BASE="http://127.0.0.1:8765"
start_server "$WORK_DIR/server" "$BASE/"

check "GET /"                "Hello, World!"   "$BASE/"
check "GET /json"            "{}"              "$BASE/json"
check "GET /html"            "<h1>Hello</h1>"  "$BASE/html"
check "GET /users/:id"       "42"              "$BASE/users/42"
check "GET /search?q="       "hi"              "$BASE/search?q=hi"
check "GET /echo (header)"   "hdr"             -H "X-Test: hdr" "$BASE/echo"

stop_server

echo ""
echo "=== SSE integration ==="
"$QUADC" "$PROJECT_ROOT/tests/qd/http/sse_integration.qd" -o "$WORK_DIR/sse" > "$WORK_DIR/build.log" 2>&1 || {
    echo -e "${RED}FAIL${NC}: could not compile sse_integration.qd"
    cat "$WORK_DIR/build.log"
    exit 1
}

SSE="http://127.0.0.1:19877/test"
start_server "$WORK_DIR/sse" "$SSE/single"

check "SSE single"    "data: hello world"                                             "$SSE/single"
check "SSE multiple"  "$(printf 'data: first\n\ndata: second\n\ndata: third')"        "$SSE/multiple"
check "SSE named"     "$(printf 'event: greeting\ndata: hello\n\nevent: farewell\ndata: goodbye')" "$SSE/named"
check "SSE empty"     ""                                                              "$SSE/empty"
check "SSE special"   "$(printf 'data: hello:world\n\ndata: key=value\n\ndata: spaces   and\ttabs')" "$SSE/special"
check "SSE long"      "data: This is a longer message that contains more data than a typical short event. It tests that the SSE implementation handles larger payloads correctly without truncation or corruption." "$SSE/long"
check "SSE loop"      "$(printf 'data: 1\n\ndata: 2\n\ndata: 3\n\ndata: 4\n\ndata: 5')"           "$SSE/loop"

stop_server

echo ""
if [ "$TESTS_FAILED" -gt 0 ]; then
    echo -e "${RED}FAILED${NC}: $TESTS_FAILED of $TESTS_RUN checks failed"
    exit 1
fi
echo -e "${GREEN}PASSED${NC}: all $TESTS_RUN checks passed"
exit 0
