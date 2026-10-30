#!/bin/bash

# Test runner for mTLS (mutual TLS) functionality
# Sets up a local TLS server requiring client certificates and tests connection
# Usage: run_mtls_test.sh [valgrind]

set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Check if running with valgrind
USE_VALGRIND="${1:-no}"
VALGRIND_CMD=""
if [ "$USE_VALGRIND" = "valgrind" ]; then
    VALGRIND_CMD="valgrind --leak-check=full --show-leak-kinds=definite,indirect,possible --track-fds=yes --track-origins=yes --error-exitcode=1 --quiet"
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

# Temp directory for certificates
CERT_DIR=$(mktemp -d)
SERVER_PID=""

cleanup() {
    rm -rf "$CERT_DIR"
    if [ -n "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null || true
    fi
}
trap cleanup EXIT

# Compiler path
QUADC="${QUADC:-$PROJECT_ROOT/build/debug/cmd/quadc/quadc}"
if [ ! -x "$QUADC" ]; then
    echo -e "${RED}FAIL${NC}: quadc not found at $QUADC"
    exit 1
fi

# Build include flags for modules (net is stdlib, tls is in-tree or external)
INCLUDE_FLAGS=""
if [ -n "${QUADRATE_EXTERNAL_MODULES:-}" ]; then
    INCLUDE_FLAGS="-I $QUADRATE_EXTERNAL_MODULES"
fi

# The tls module ships in dist/share/quadrate after `make`. This looked in lib/tls, which
# the module left when it moved to stdlib/, so the test skipped and still counted as a pass.
TLS_FOUND=0
if [ -f "$PROJECT_ROOT/dist/share/quadrate/tls/tls.qd" ]; then
    export QUADRATE_ROOT="$PROJECT_ROOT/dist/share/quadrate"
    TLS_FOUND=1
elif [ -n "${QUADRATE_EXTERNAL_MODULES:-}" ] && [ -f "$QUADRATE_EXTERNAL_MODULES/tls/tls.qd" ]; then
    TLS_FOUND=1
fi

if [ "$TLS_FOUND" -eq 0 ]; then
    echo -e "${RED}SKIP${NC}: tls module not found (not in dist/share/quadrate or QUADRATE_EXTERNAL_MODULES)"
    exit 0
fi

# Check for openssl
if ! command -v openssl &> /dev/null; then
    echo -e "${RED}SKIP${NC}: openssl not found, skipping mTLS test"
    exit 0
fi

echo "=== Running mTLS Test ==="
echo ""

# Step 1: Generate certificates
echo -n "Generating test certificates... "

# Create CA
openssl genrsa -out "$CERT_DIR/ca.key" 2048 2>/dev/null
openssl req -new -x509 -days 1 -key "$CERT_DIR/ca.key" -out "$CERT_DIR/ca.crt" \
    -subj "/CN=Test CA" 2>/dev/null

# Create server cert
openssl genrsa -out "$CERT_DIR/server.key" 2048 2>/dev/null
openssl req -new -key "$CERT_DIR/server.key" -out "$CERT_DIR/server.csr" \
    -subj "/CN=localhost" 2>/dev/null
openssl x509 -req -days 1 -in "$CERT_DIR/server.csr" -CA "$CERT_DIR/ca.crt" \
    -CAkey "$CERT_DIR/ca.key" -CAcreateserial -out "$CERT_DIR/server.crt" 2>/dev/null

# Create client cert
openssl genrsa -out "$CERT_DIR/client.key" 2048 2>/dev/null
openssl req -new -key "$CERT_DIR/client.key" -out "$CERT_DIR/client.csr" \
    -subj "/CN=test-client" 2>/dev/null
openssl x509 -req -days 1 -in "$CERT_DIR/client.csr" -CA "$CERT_DIR/ca.crt" \
    -CAkey "$CERT_DIR/ca.key" -CAcreateserial -out "$CERT_DIR/client.crt" 2>/dev/null

echo -e "${GREEN}done${NC}"

# Step 2: Start OpenSSL server
echo -n "Starting TLS server (port 18443)... "

# Find an available port (use fixed port for simplicity, high port to avoid conflicts)
PORT=18443

# Start server in background, requiring client certificates
openssl s_server -accept $PORT \
    -cert "$CERT_DIR/server.crt" \
    -key "$CERT_DIR/server.key" \
    -CAfile "$CERT_DIR/ca.crt" \
    -Verify 1 \
    -quiet \
    > "$CERT_DIR/server.log" 2>&1 &
SERVER_PID=$!

# Wait for server to start
sleep 1

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}FAIL${NC}"
    echo "Server failed to start. Log:"
    cat "$CERT_DIR/server.log"
    exit 1
fi

echo -e "${GREEN}done${NC} (PID: $SERVER_PID)"

# Step 3: Create test program
echo -n "Creating test program... "

cat > "$CERT_DIR/mtls_test.qd" << 'ENDOFQD'
use net
use tls

fn main( -- ) {
ENDOFQD

# Add dynamic paths using shell
cat >> "$CERT_DIR/mtls_test.qd" << EOF
    // Connect to local server
    "127.0.0.1" $PORT net::connect! -> sock

    // Connect with mTLS; the test runs with SSL_CERT_FILE set to the test CA
    sock "localhost" "$CERT_DIR/client.crt" "$CERT_DIR/client.key" tls::connect_mtls switch {
        Ok {
            -> conn
            "MTLS_CONNECTED" print nl
            conn tls::close
        }
        _ {
            err -> code -> msg
            "MTLS_FAILED " print msg print nl
        }
    }

    sock net::close
    "MTLS_DONE" print nl
}
EOF

echo -e "${GREEN}done${NC}"

# Step 4: Compile and run test
echo -n "Running mTLS connection test... "

# Compile
if ! "$QUADC" $INCLUDE_FLAGS -o "$CERT_DIR/mtls_test" "$CERT_DIR/mtls_test.qd" 2>"$CERT_DIR/compile.log"; then
    echo -e "${RED}FAIL${NC} (compile error)"
    cat "$CERT_DIR/compile.log"
    exit 1
fi

# Run test, trusting the test CA. Without it the handshake failed on the certificate and
# the test passed anyway, so it never showed that a client certificate was accepted.
if output=$(SSL_CERT_FILE="$CERT_DIR/ca.crt" $VALGRIND_CMD "$CERT_DIR/mtls_test" 2>&1); then
    if echo "$output" | grep -q "MTLS_CONNECTED" && echo "$output" | grep -q "MTLS_DONE"; then
        echo -e "${GREEN}PASS${NC}"
        echo ""
        echo "=== Test Output ==="
        echo "$output"
        exit 0
    fi
    echo -e "${RED}FAIL${NC} (unexpected output)"
    echo "Output:"
    echo "$output"
    exit 1
else
    echo -e "${RED}FAIL${NC} (execution error)"
    echo "Output:"
    echo "$output"
    echo ""
    echo "Server log:"
    cat "$CERT_DIR/server.log"
    exit 1
fi
