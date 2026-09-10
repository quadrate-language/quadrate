#!/usr/bin/env bash

set -eu

# Flags, in any order:
#   --clang-format  run `make format` and fail on a diff
#   --valgrind      run the suite under valgrind
#
# The clang-format check belongs to exactly one job. Every image ships a
# different rolling clang-format (alpine/edge 23.x, archlinux 22.x, debian/testing
# its own), and versions disagree irreconcilably on some constructs -- 22 emits
# `{ // c` for a comment opening a braced initialiser and 23 emits `{// c`, with
# neither accepting the other's output. Run on all three, this job stops testing
# "is the code formatted" and starts testing "does this image's clang-format agree
# with whoever last ran make format", which fails on a new release through nobody's
# fault. Formatting is not platform-specific; build and test are, and those still
# run everywhere.
run_valgrind=0
run_clang_format=0
for arg in "$@"; do
	case "$arg" in
		--valgrind) run_valgrind=1 ;;
		--clang-format) run_clang_format=1 ;;
		*)
			echo "ci-checks: unknown option: $arg" >&2
			exit 2
			;;
	esac
done

git config --global --add safe.directory "$PWD"

if [ "$run_clang_format" = 1 ]; then
	echo "::group::format"
	clang-format --version
	make format
	git diff --exit-code
	echo "::endgroup::"
fi

echo "::group::build"
make release
echo "::endgroup::"

# quadfmt is built from this tree, so it is the same formatter on every image --
# unlike clang-format above, this check is not version-sensitive.
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
