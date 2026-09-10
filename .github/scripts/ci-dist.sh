#!/usr/bin/env bash

set -eu

libc="${1:?usage: ci-dist.sh <libc-label>}"

git config --global --add safe.directory "$PWD"

make dist

# A release artefact must not be stamped from a dirty tree: the version string is
# baked into every binary, and "0.5.0-28-gabc1234-dirty" tells a user nothing they
# can check out.
if ! git diff --quiet HEAD 2>/dev/null; then
	echo "ci-dist: refusing to build a release from a dirty working tree" >&2
	git status --porcelain >&2
	exit 1
fi

version=$(git describe --tags --abbrev=0 2>/dev/null || echo 0.0.0-unknown)
arch=$(uname -m)
src="quadrate-$version-linux-$arch.tar.gz"
dst="quadrate-$version-linux-$arch-$libc.tar.gz"

mkdir -p dist-out
mv "$src" "dist-out/$dst"
(cd dist-out && sha256sum "$dst" >"$dst.sha256")

echo "Staged dist-out/$dst"
