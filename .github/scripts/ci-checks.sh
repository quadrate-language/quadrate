#!/usr/bin/env bash

set -eu

run_valgrind=0
if [ "${1:-}" = "--valgrind" ]; then
	run_valgrind=1
fi

git config --global --add safe.directory "$PWD"

echo "::group::format"
make format
git diff --exit-code
echo "::endgroup::"

echo "::group::build"
make release
echo "::endgroup::"

echo "::group::fmtcheck"
make fmtcheck
echo "::endgroup::"

echo "::group::docscheck"
make docscheck
echo "::endgroup::"

echo "::group::test"
make tests
echo "::endgroup::"

if [ "$run_valgrind" = 1 ]; then
	echo "::group::valgrind"
	make valgrind
	echo "::endgroup::"
fi
