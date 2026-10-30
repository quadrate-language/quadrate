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
elif [ -d "$PROJECT_ROOT/stdlib/http/qd/http" ]; then
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
# curl sends the body only once the server answers "100 Continue"; the server read once
# and saw none of it
check "POST, body after 100"  "payload"        -H "Expect: 100-continue" --data-binary "payload" "$BASE/body"
# A response head over 4 KB overran the buffer it was formatted into
check "6000-byte header"      "$(head -c 6000 /dev/zero | tr '\0' x)" -o /dev/null -w '%header{x-big}' "$BASE/big-header"

echo ""
echo "=== HTTP client integration ==="
"$QUADC" "$PROJECT_ROOT/tests/qd/http/client_integration.qd" -o "$WORK_DIR/client" > "$WORK_DIR/build.log" 2>&1 || {
    echo -e "${RED}FAIL${NC}: could not compile client_integration.qd"
    cat "$WORK_DIR/build.log"
    exit 1
}

# client_check <name> <expected> <client-args...>
client_check() {
    local name="$1" expected="$2"
    shift 2
    local actual
    TESTS_RUN=$((TESTS_RUN + 1))
    actual=$(timeout 10 "$WORK_DIR/client" "$@" 2>&1 || true)
    if [ "$actual" = "$expected" ]; then
        echo -e "  ${GREEN}✓${NC} $name"
    else
        echo -e "  ${RED}✗${NC} $name"
        echo "      expected: $(printf '%q' "$expected")"
        echo "      actual:   $(printf '%q' "$actual")"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

client_check "GET"                 "$(printf '200\nHello, World!')"  "$BASE/"
client_check "GET with a header"   "$(printf '200\nfrom-client')"    "$BASE/echo"
client_check "POST with a body"    "$(printf '200\npayload')"        "$BASE/body" "payload"
client_check "POST an empty body"  "200"                              "$BASE/body" ""
client_check "nothing listening"   "error 2"                          "http://127.0.0.1:1/"
client_check "not an http URL"     "error 6"                          "ftp://127.0.0.1/"

stop_server

# A chunked response, which the client must decode
if command -v python3 > /dev/null 2>&1; then
    cat > "$WORK_DIR/chunked.py" << 'ENDOFPY'
import http.server
class Chunked(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def do_GET(self):
        self.send_response(200)
        self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()
        for part in (b"hello ", b"chunked ", b"world"):
            self.wfile.write(b"%x\r\n%s\r\n" % (len(part), part))
        self.wfile.write(b"0\r\n\r\n")
        self.close_connection = True
    def log_message(self, *args):
        pass
http.server.HTTPServer(("127.0.0.1", 18766), Chunked).serve_forever()
ENDOFPY
    python3 -I "$WORK_DIR/chunked.py" > "$WORK_DIR/server.log" 2>&1 &
    SERVER_PID=$!
    for i in $(seq 1 50); do
        curl -s -o /dev/null --max-time 2 "http://127.0.0.1:18766/" 2>/dev/null && break
        sleep 0.1
    done
    client_check "chunked response" "$(printf '200\nhello chunked world')" "http://127.0.0.1:18766/"
    stop_server
fi

# https, against a certificate from a throwaway CA the client is told to trust
if command -v openssl > /dev/null 2>&1; then
    openssl req -x509 -newkey rsa:2048 -nodes -keyout "$WORK_DIR/ca.key" -out "$WORK_DIR/ca.crt" \
        -days 1 -subj "/CN=Test CA" > /dev/null 2>&1
    openssl req -newkey rsa:2048 -nodes -keyout "$WORK_DIR/srv.key" -out "$WORK_DIR/srv.csr" \
        -subj "/CN=localhost" > /dev/null 2>&1
    printf 'subjectAltName=DNS:localhost\n' > "$WORK_DIR/srv.ext"
    openssl x509 -req -in "$WORK_DIR/srv.csr" -CA "$WORK_DIR/ca.crt" -CAkey "$WORK_DIR/ca.key" \
        -CAcreateserial -out "$WORK_DIR/srv.crt" -days 1 -extfile "$WORK_DIR/srv.ext" > /dev/null 2>&1
    openssl s_server -accept 18767 -cert "$WORK_DIR/srv.crt" -key "$WORK_DIR/srv.key" -www -quiet \
        > "$WORK_DIR/server.log" 2>&1 &
    SERVER_PID=$!
    for i in $(seq 1 50); do
        curl -sk -o /dev/null --max-time 2 "https://localhost:18767/" 2>/dev/null && break
        sleep 0.1
    done
    TESTS_RUN=$((TESTS_RUN + 1))
    status=$(SSL_CERT_FILE="$WORK_DIR/ca.crt" timeout 10 "$WORK_DIR/client" "https://localhost:18767/" 2>&1 | head -n 1 || true)
    if [ "$status" = "200" ]; then
        echo -e "  ${GREEN}✓${NC} https"
    else
        echo -e "  ${RED}✗${NC} https"
        echo "      expected: 200"
        echo "      actual:   $(printf '%q' "$status")"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    client_check "https, untrusted"  "error 3"  "https://localhost:18767/"
    stop_server
fi

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
