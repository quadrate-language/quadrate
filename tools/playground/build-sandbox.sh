#!/bin/bash
# Build the sandbox Docker image for Quadrate Playground
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
IMAGE_NAME="${1:-quadrate-sandbox}"

cd "$REPO_ROOT"

# Check if dist/ exists with required files
if [[ ! -f "dist/bin/quadc" ]]; then
    echo "Error: dist/bin/quadc not found. Run 'make release' first."
    exit 1
fi

# Create a temporary build context with required files
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

echo "Preparing build context..."
if [ "$(uname -s)" = "Haiku" ]; then
    DIST_DATADIR="dist/data"
else
    DIST_DATADIR="dist/share"
fi
mkdir -p "$TMPDIR/dist/bin" "$TMPDIR/dist/lib" "$TMPDIR/$DIST_DATADIR" "$TMPDIR/libs"
cp dist/bin/quadc "$TMPDIR/dist/bin/"
cp dist/lib/*.so "$TMPDIR/dist/lib/" 2>/dev/null || true
cp dist/lib/*.a "$TMPDIR/dist/lib/" 2>/dev/null || true
cp -r "$DIST_DATADIR/quadrate" "$TMPDIR/$DIST_DATADIR/"

# Copy shared library dependencies that quadc needs
# Exclude core system libraries that come from the base image
echo "Copying shared library dependencies..."
ldd dist/bin/quadc | grep "=> /" | awk '{print $3}' | while read lib; do
    name=$(basename "$lib")
    # Skip core glibc libraries - use container's versions
    case "$name" in
        libc.so*|libm.so*|libpthread.so*|libdl.so*|librt.so*|ld-linux*.so*)
            echo "  [skip] $lib (system library)"
            ;;
        *)
            if [[ -f "$lib" ]]; then
                echo "  $lib"
                cp "$lib" "$TMPDIR/libs/"
            fi
            ;;
    esac
done

cp tools/playground/Dockerfile.sandbox "$TMPDIR/Dockerfile"

echo "Building Docker image: $IMAGE_NAME"
docker build -t "$IMAGE_NAME" "$TMPDIR"

echo ""
echo "Done! Run the playground with:"
echo "  cd tools/playground"
echo "  go build -o playground ."
echo "  ./playground -sandbox"
