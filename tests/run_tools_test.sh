#!/bin/bash

# Smoke and behaviour tests for quaduses, quaddoc and quadrepl.
#
# These three tools had no tests and no suite at all -- ~2700 LOC between them.
# Both bugs found in the 2026-09-10 review lived here: quaddoc aborting with an
# uncaught filesystem_error on a bad path, and tools exiting 0 on input they
# never read. The regressions for those are at the bottom of each section.
#
# Usage: run_tools_test.sh

set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

WORK_DIR=$(mktemp -d)
cleanup() { rm -rf "$WORK_DIR"; }
trap cleanup EXIT

BUILD_DIR="${BUILD_DIR:-build/debug}"
QUADUSES="${QUADUSES:-$PROJECT_ROOT/$BUILD_DIR/cmd/quaduses/quaduses}"
QUADDOC="${QUADDOC:-$PROJECT_ROOT/$BUILD_DIR/cmd/quaddoc/quaddoc}"
QUADREPL="${QUADREPL:-$PROJECT_ROOT/$BUILD_DIR/cmd/quadrepl/quadrepl}"
QUADFMT="${QUADFMT:-$PROJECT_ROOT/$BUILD_DIR/cmd/quadfmt/quadfmt}"

# Marked with the same U+2717 as a failing check: run_all.sh extracts the message
# it reports by grepping for that character, so a bare "FAIL" here surfaced as
# "0 passed, 1 failed" with no reason attached. A tool is missing rather than
# broken when its meson.build skipped it -- quadrepl needs readline.
for tool in "$QUADUSES" "$QUADDOC" "$QUADREPL" "$QUADFMT"; do
    if [ ! -x "$tool" ]; then
        echo -e "  ${RED}\u2717${NC} $tool not found (not built? check meson output for a skip warning)"
        exit 1
    fi
done

TESTS_RUN=0
TESTS_FAILED=0

pass() { echo -e "  ${GREEN}✓${NC} $1"; TESTS_RUN=$((TESTS_RUN + 1)); }
fail() {
    echo -e "  ${RED}✗${NC} $1"
    [ -n "${2:-}" ] && echo "      $2"
    TESTS_RUN=$((TESTS_RUN + 1))
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

# expect_rc <name> <expected-rc> <command...>
expect_rc() {
    local name="$1" want="$2"; shift 2
    local got
    "$@" > "$WORK_DIR/out" 2>&1 && got=0 || got=$?
    if [ "$got" = "$want" ]; then pass "$name"; else
        fail "$name" "expected rc=$want, got rc=$got: $(head -2 "$WORK_DIR/out" | tr '\n' ' ')"
    fi
}

# expect_contains <name> <needle> <command...>
expect_contains() {
    local name="$1" needle="$2"; shift 2
    local out
    out=$("$@" 2>&1 || true)
    if echo "$out" | grep -qF -- "$needle"; then pass "$name"; else
        fail "$name" "missing '$needle' in: $(echo "$out" | head -2 | tr '\n' ' ')"
    fi
}

# expect_not_contains <name> <needle> <command...>
expect_not_contains() {
    local name="$1" needle="$2"; shift 2
    local out
    out=$("$@" 2>&1 || true)
    if echo "$out" | grep -qF -- "$needle"; then
        fail "$name" "unexpectedly found '$needle'"
    else pass "$name"; fi
}

echo "=== quaduses ==="

printf 'fn main() {\n\t"a,b" "," strings::split -> _ -> parts\n\tparts print nl\n}\n' > "$WORK_DIR/missing.qd"
printf 'use math\n\nfn main() {\n\t1 print nl\n}\n' > "$WORK_DIR/unused.qd"
printf 'use strings\n\nfn main() {\n\t"a" "b" strings::concat print nl\n}\n' > "$WORK_DIR/clean.qd"

expect_contains     "adds a missing use"        "use strings"  "$QUADUSES" "$WORK_DIR/missing.qd"
expect_not_contains "removes an unused use"     "use math"     "$QUADUSES" "$WORK_DIR/unused.qd"
expect_rc           "--check flags a file needing changes" 1    "$QUADUSES" -c "$WORK_DIR/missing.qd"
expect_rc           "--check passes a clean file"          0    "$QUADUSES" -c "$WORK_DIR/clean.qd"

cp "$WORK_DIR/missing.qd" "$WORK_DIR/inplace.qd"
"$QUADUSES" -w "$WORK_DIR/inplace.qd" > /dev/null 2>&1 || true
if grep -q "use strings" "$WORK_DIR/inplace.qd"; then pass "--write updates in place"; else
    fail "--write updates in place" "file unchanged"; fi

expect_rc "missing file exits non-zero" 1 "$QUADUSES" "$WORK_DIR/does-not-exist.qd"

echo ""
echo "=== quaddoc ==="

mkdir -p "$WORK_DIR/src/greet"
cat > "$WORK_DIR/src/greet/greet.qd" <<'EOF'
/// Greeting helpers.

/// Return a greeting for a name.
pub fn hello(name:str -- msg:str) {
	"Hello, " name strings::concat
}
EOF

"$QUADDOC" -q -o "$WORK_DIR/docs" "$WORK_DIR/src" > /dev/null 2>&1 || true
for f in index.html greet.html style.css; do
    if [ -f "$WORK_DIR/docs/$f" ]; then pass "generates $f"; else fail "generates $f" "not created"; fi
done
if grep -q "hello" "$WORK_DIR/docs/greet.html" 2>/dev/null; then pass "module page documents the function"; else
    fail "module page documents the function" "'hello' absent from greet.html"; fi

# Regression: these aborted with an uncaught std::filesystem_error (rc=134).
expect_rc       "missing path exits 1, does not abort"      1 "$QUADDOC" -q "$WORK_DIR/no-such-dir"
expect_contains "missing path reports it as quaddoc"        "quaddoc: " \
                "$QUADDOC" -q "$WORK_DIR/no-such-dir"
expect_not_contains "missing path does not throw"           "terminate called" \
                "$QUADDOC" -q "$WORK_DIR/no-such-dir"
expect_rc       "file argument exits 1, does not abort"     1 "$QUADDOC" -q "$WORK_DIR/src/greet/greet.qd"
expect_contains "file argument reports it as quaddoc"       "quaddoc: " \
                "$QUADDOC" -q "$WORK_DIR/src/greet/greet.qd"
expect_not_contains "file argument does not throw"          "terminate called" \
                "$QUADDOC" -q "$WORK_DIR/src/greet/greet.qd"

echo ""
echo "=== quadrepl ==="

out=$(printf '2 3 add\n' | timeout 10 "$QUADREPL" 2>&1 | tail -1 || true)
if [ "$out" = "5" ]; then pass "evaluates piped input"; else
    fail "evaluates piped input" "expected 5, got '$out'"; fi

out=$(printf '"hi" print\n' | timeout 10 "$QUADREPL" 2>&1 | tail -1 || true)
if [ "$out" = "hi" ]; then pass "runs instructions from a pipe"; else
    fail "runs instructions from a pipe" "expected hi, got '$out'"; fi

out=$(printf '1 2 add\n' | timeout 10 "$QUADREPL" -p 2>&1 | tail -1 || true)
if [ "$out" = "3" ]; then pass "--print reports the stack"; else
    fail "--print reports the stack" "expected 3, got '$out'"; fi

echo ""
echo "=== quadfmt stdin ==="

printf 'fn  main( ) {\n1 if {\n2 print nl\n}\n}\n' > "$WORK_DIR/fmt.qd"

if diff -q <("$QUADFMT" "$WORK_DIR/fmt.qd") <("$QUADFMT" - < "$WORK_DIR/fmt.qd") > /dev/null 2>&1; then
    pass "stdin and file produce identical output"
else
    fail "stdin and file produce identical output" "outputs differ"
fi

expect_rc "stdin formatting exits 0" 0 sh -c "'$QUADFMT' - < '$WORK_DIR/fmt.qd'"

# An editor replaces its buffer with our stdout, so unparseable input must come
# back unchanged rather than as nothing.
out=$(printf 'fn main() { @@@ }\n' | "$QUADFMT" - 2>/dev/null || true)
if [ "$out" = "fn main() { @@@ }" ]; then pass "unparseable stdin is echoed back unchanged"; else
    fail "unparseable stdin is echoed back unchanged" "got '$out'"; fi

expect_rc "unparseable stdin exits 1" 1 sh -c "printf 'fn main() { @@@ }\n' | '$QUADFMT' -"

echo ""
echo "=== quadc output naming ==="

QUADC="${QUADC:-$PROJECT_ROOT/$BUILD_DIR/cmd/quadc/quadc}"
# quadc resolves the runtime relative to its own location; run from a temp dir it
# would look under ~/.local/lib, so point it at the tree explicitly.
export QUADRATE_LIBDIR="${QUADRATE_LIBDIR:-$PROJECT_ROOT/dist/lib}"
export QUADRATE_ROOT="${QUADRATE_ROOT:-$PROJECT_ROOT/dist/share/quadrate}"
mkdir -p "$WORK_DIR/nameA" "$WORK_DIR/nameB"
printf 'fn main() { "A" print nl }\n' > "$WORK_DIR/nameA/alpha.qd"
printf 'fn main() { "B" print nl }\n' > "$WORK_DIR/nameB/beta.qd"

# The default used to be a hardcoded "main" -- the entry function's name -- so
# two sources in one directory silently overwrote each other's binary.
(cd "$WORK_DIR/nameA" && "$QUADC" alpha.qd > /dev/null 2>&1) || true
(cd "$WORK_DIR/nameB" && "$QUADC" beta.qd  > /dev/null 2>&1) || true

if [ -x "$WORK_DIR/nameA/alpha" ]; then pass "quadc names the binary after the source"; else
    fail "quadc names the binary after the source" "expected alpha, got: $(ls "$WORK_DIR/nameA")"; fi
if [ ! -e "$WORK_DIR/nameA/main" ]; then pass "quadc no longer produces 'main'"; else
    fail "quadc no longer produces 'main'" "'main' was created"; fi
if [ -x "$WORK_DIR/nameB/beta" ]; then pass "a second source gets its own name"; else
    fail "a second source gets its own name" "expected beta, got: $(ls "$WORK_DIR/nameB")"; fi

(cd "$WORK_DIR/nameA" && "$QUADC" alpha.qd -o chosen > /dev/null 2>&1) || true
if [ -x "$WORK_DIR/nameA/chosen" ]; then pass "-o still overrides the default"; else
    fail "-o still overrides the default" "no 'chosen' produced"; fi

echo ""
echo "=== io::readline end-of-input (needs controlled stdin) ==="

# Lives here rather than in the qd suite because that suite inherits stdin: a
# readline test there would block on a terminal. Running out of input is how a
# read loop ends, so it must not set the error state -- it used to, and because
# Quadrate cannot clear that state a *successful* drain reported failure to its
# caller. examples/wc/wc.qd carried a comment about hand-rolling around it.
mkdir -p "$WORK_DIR/rl"
cat > "$WORK_DIR/rl/readline.qd" <<'ENDQD'
use io

fn count_lines( -- n:i64)! {
	0 -> n
	loop {
		io::readline switch {
			Ok { -> ok -> line ok 0 == if { line drop break } line drop n 1 + -> n }
			_ { "READ-FAILURE" print nl break }
		}
	}
	n
}

fn main() {
	count_lines switch {
		Ok { -> n "counted=" print n print nl }
		_ { "caller-saw-failure" print nl }
	}
	"errcode=" print error <<code print nl
}
ENDQD

if "$QUADC" "$WORK_DIR/rl/readline.qd" -o "$WORK_DIR/rl/readline" > /dev/null 2>&1; then
    out=$(printf 'a\nb\nc\n' | "$WORK_DIR/rl/readline" 2>&1)
    if echo "$out" | grep -q "counted=3"; then pass "a full drain reports success"; else
        fail "a full drain reports success" "$(echo "$out" | tr '\n' ' ')"; fi
    if echo "$out" | grep -q "errcode=0"; then pass "end of input leaves no error state"; else
        fail "end of input leaves no error state" "$(echo "$out" | tr '\n' ' ')"; fi
    if echo "$out" | grep -q "caller-saw-failure"; then
        fail "caller must not see a failure" "$(echo "$out" | tr '\n' ' ')"
    else pass "the caller is not told it failed"; fi

    out_empty=$(printf '' | "$WORK_DIR/rl/readline" 2>&1)
    if echo "$out_empty" | grep -q "counted=0"; then pass "empty stdin counts zero lines"; else
        fail "empty stdin counts zero lines" "$(echo "$out_empty" | tr '\n' ' ')"; fi
    if echo "$out_empty" | grep -q "errcode=0"; then pass "empty stdin leaves no error state"; else
        fail "empty stdin leaves no error state" "$(echo "$out_empty" | tr '\n' ' ')"; fi
else
    fail "readline test program compiles" "compilation failed"
fi

echo ""
echo "=== shared CLI surface ==="

for spec in "quaduses:$QUADUSES" "quaddoc:$QUADDOC" "quadrepl:$QUADREPL"; do
    name="${spec%%:*}"; bin="${spec#*:}"
    expect_rc       "$name --help exits 0"        0 "$bin" --help
    expect_contains "$name --help names the tool" "$name" "$bin" --help
    expect_rc       "$name --version exits 0"     0 "$bin" --version
    expect_contains "$name --version reports it"  "$name" "$bin" --version
    expect_rc       "$name rejects unknown options" 1 "$bin" --definitely-not-a-flag
done

echo ""
if [ "$TESTS_FAILED" -gt 0 ]; then
    echo -e "${RED}FAILED${NC}: $TESTS_FAILED of $TESTS_RUN checks failed"
    exit 1
fi
echo -e "${GREEN}PASSED${NC}: all $TESTS_RUN checks passed"
exit 0
