#!/usr/bin/env bash

set -eu

libc="${1:?usage: ci-dist.sh <libc-label>}"

git config --global --add safe.directory "$PWD"

make dist

version=$(git describe --tags --abbrev=0 2>/dev/null || echo 0.0.0-unknown)
arch=$(uname -m)
src="quadrate-$version-linux-$arch.tar.gz"
dst="quadrate-$version-linux-$arch-$libc.tar.gz"

mkdir -p dist-out
mv "$src" "dist-out/$dst"
(cd dist-out && sha256sum "$dst" >"$dst.sha256")

echo "Staged dist-out/$dst"
