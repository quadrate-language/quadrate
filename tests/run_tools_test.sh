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
QUADLINT="${QUADLINT:-$PROJECT_ROOT/$BUILD_DIR/cmd/quadlint/quadlint}"

# Marked with the same U+2717 as a failing check: run_all.sh extracts the message
# it reports by grepping for that character, so a bare "FAIL" here surfaced as
# "0 passed, 1 failed" with no reason attached. A tool is missing rather than
# broken when its meson.build skipped it -- quadrepl needs readline.
for tool in "$QUADUSES" "$QUADDOC" "$QUADREPL" "$QUADFMT" "$QUADLINT"; do
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

# quaddoc reads declarations off the compiler's AST. It used to match them with
# hand-written regular expressions, which silently dropped every form the
# patterns had not anticipated -- the whole `sys` module and the whole `ct`
# container library were missing from the published documentation. Each check
# below is one of those forms.
mkdir -p "$WORK_DIR/src/shapes"
cat > "$WORK_DIR/src/shapes/shapes.qd" <<'QDEOF'
/// Shapes.

pub struct Box { w: i64 }

/// Inline functions are still functions.
/// @param p i64 A port
pub inline fn emit(p:i64 -- ) {
	p drop
}

/// A generic function.
pub fn (b:Box) scaled<T>(k:T -- out:Box) {
	b
}

/// Takes a function.
/// @param f fn(i64 -- i64) The callback
pub fn apply(x:i64 f:fn(i64 -- i64) -- y:i64) {
	x f call
}

/// A method on a struct.
pub fn (b:Box) width( -- n:i64) {
	b <<w
}

/// Mentions width only in a comment.
pub fn describe( -- n:i64) {
	// returns the width of the box
	7
}
QDEOF

"$QUADDOC" -q -o "$WORK_DIR/docs2" "$WORK_DIR/src/shapes" > /dev/null 2>&1 || true
SHAPES="$WORK_DIR/docs2/shapes.html"

if grep -q 'id="emit"' "$SHAPES" 2>/dev/null; then pass "documents 'pub inline fn'"; else
    fail "documents 'pub inline fn'" "the whole sys module went missing this way"; fi
if grep -q 'id="scaled"' "$SHAPES" 2>/dev/null; then pass "documents a generic function"; else
    fail "documents a generic function" "the whole ct container library went missing this way"; fi

# The old pattern ended the parameter list at the first ')', truncating the
# signature mid-type and losing everything after it.
expect_contains "function-pointer parameter type is not truncated" \
                "f:fn(i64 -- i64) -- y:i64)" cat "$SHAPES"
# `fn b:Box width()` is not syntax anyone can paste back into a file.
expect_contains "receiver is parenthesised in the signature" \
                "fn (b:Box) width( -- n:i64)" cat "$SHAPES"
# A word in a comment is not a call: the old scanner regexed body *text*.
expect_not_contains "a name mentioned in a comment is not a call" \
                "#width" grep -A6 'id="describe"' "$SHAPES"

# A file that does not parse is reported rather than published half-empty.
mkdir -p "$WORK_DIR/src/broken"
printf '/// Broken.\n\npub fn ok(x:i64 -- y:i64) {\n\tx\n}\n\npub fn bad(  {\n' \
    > "$WORK_DIR/src/broken/broken.qd"
expect_contains "reports a parse error as a located warning" \
                ": warning: " "$QUADDOC" -o "$WORK_DIR/docs3" "$WORK_DIR/src/broken"
expect_not_contains "-q silences the parse warning" \
                ": warning: " "$QUADDOC" -q -o "$WORK_DIR/docs3" "$WORK_DIR/src/broken"


echo ""
echo "=== quadrepl ==="

# The REPL's answer is the last line it writes, but the check keeps the whole
# output to report. It JITs each line, which means compiling and linking against
# librt: when that fails -- a rebuild rewriting the archive underneath the suite
# will do it -- the reason is in the lines above, and reporting only the last
# one reduced every test here to "expected 5, got ''".
repl_last() {
    local name="$1" want="$2"; shift 2
    local all last
    all=$(printf '%b' "$1" | timeout 10 "$QUADREPL" "${@:2}" 2>&1 || true)
    last=$(printf '%s' "$all" | tail -1)
    if [ "$last" = "$want" ]; then pass "$name"; else
        fail "$name" "expected $want, got '$last'; full output: $(printf '%s' "$all" | tr '\n' ' ')"; fi
}

repl_last "evaluates piped input" 5 '2 3 add\n'
repl_last "runs instructions from a pipe" hi '"hi" print\n'
repl_last "--print reports the stack" 3 '1 2 add\n' -p

repl() { printf '%b' "$1" | timeout 20 "$QUADREPL" 2>&1; }

out=$(repl '5\n3\nadd\nstack\n')
if echo "$out" | grep -q '^  \[0\] 8$'; then pass "values stay on the stack between lines"; else
    fail "values stay on the stack between lines" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl '5 3\nstack\n')
if [ "$(echo "$out" | grep -c '^  \[')" = "2" ]; then pass "one line can leave several values"; else
    fail "one line can leave several values" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl '"a" 2.5\nstack\n')
if echo "$out" | grep -q '\[1\] 2.5$'; then pass "floats print without trailing zeros"; else
    fail "floats print without trailing zeros" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl '"hello print world" prints\n')
if echo "$out" | grep -q 'hello print world'; then pass "a string literal containing 'print' is untouched"; else
    fail "a string literal containing 'print' is untouched" "$(echo "$out" | tr '\n' ' ')"; fi
if echo "$out" | grep -q 'print nl'; then
    fail "no 'nl' is spliced into a string literal" "$(echo "$out" | tr '\n' ' ')"
else
    pass "no 'nl' is spliced into a string literal"; fi

out=$(repl '5 3 add\nreset\nstack\n')
if echo "$out" | grep -q 'Stack is empty'; then pass "reset clears the stack"; else
    fail "reset clears the stack" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl '1 2 add\n:quit\n3 4 add\n')
if [ "$(echo "$out" | tail -1)" = "3" ]; then pass ":quit ends the session"; else
    fail ":quit ends the session" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl '1 2 3 clear\nstack\n')
if echo "$out" | grep -q 'Stack is empty'; then pass "'clear' inside a line runs as an instruction"; else
    fail "'clear' inside a line runs as an instruction" "$(echo "$out" | tr '\n' ' ')"; fi

out_dir="$WORK_DIR/replay"; mkdir -p "$out_dir"
(cd "$out_dir" && printf 'use io\n"log.txt" "line\\n" io::append_file!\n1 print\n2 print\n3 print\n' \
    | timeout 30 "$QUADREPL" > /dev/null 2>&1) || true
written=$(wc -l < "$out_dir/log.txt" 2>/dev/null || echo 0)
if [ "$written" = "1" ]; then pass "a side effect runs once, not once per later line"; else
    fail "a side effect runs once, not once per later line" "wrote $written lines, expected 1"; fi

echo ""
echo "=== quadrepl session locals ==="

out=$(repl '5 -> x\nx print\n')
if [ "$(echo "$out" | tail -1)" = "5" ]; then pass "a local outlives the line that bound it"; else
    fail "a local outlives the line that bound it" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl '5 -> x\n99 -> x\nx print\n')
if [ "$(echo "$out" | tail -1)" = "99" ]; then pass "a local can be rebound"; else
    fail "a local can be rebound" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl '1 -> a\n2 -> b\na print\nb print\n')
if [ "$(echo "$out" | tr '\n' ' ')" = "1 2 " ]; then pass "several locals coexist"; else
    fail "several locals coexist" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl '1 2\n10 -> x\nstack\n')
if [ "$(echo "$out" | grep -c '^  \[')" = "2" ]; then pass "a local is not part of the visible stack"; else
    fail "a local is not part of the visible stack" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl '1 2\n10 -> x\nx add\nstack\n')
if echo "$out" | grep -q '^  \[1\] 12$'; then pass "a local can be pushed back and used"; else
    fail "a local can be pushed back and used" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl '1 if { 42 -> inner }\ninner print\n')
if echo "$out" | grep -q "Undefined identifier 'inner'"; then pass "a binding inside a block does not leak"; else
    fail "a binding inside a block does not leak" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl '5 -> x\nreset\nx print\n')
if echo "$out" | grep -q "Undefined identifier 'x'"; then pass "reset forgets the session's locals"; else
    fail "reset forgets the session's locals" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl '1 2\n9 -> x\nclear\nx print\n')
if [ "$(echo "$out" | tail -1)" = "9" ]; then pass "clear empties the stack but keeps locals"; else
    fail "clear empties the stack but keeps locals" "$(echo "$out" | tr '\n' ' ')"; fi

echo ""
echo "=== quadrepl declarations ==="

out=$(repl 'const K = 7\nK print\n')
if [ "$(echo "$out" | tail -1)" = "7" ]; then pass "a const declaration is kept"; else
    fail "a const declaration is kept" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl 'struct Point { x:i64 y:i64 }\nPoint { x = 3 y = 4 }\n<<x print\n')
if [ "$(echo "$out" | tail -1)" = "3" ]; then pass "a struct declaration is kept"; else
    fail "a struct declaration is kept" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl 'struct Point { x:i64 y:i64 }\nPoint { x = 3 y = 4 } -> p\np <<x print\np <<y print\n')
if [ "$(echo "$out" | tr '\n' ' ' | grep -c '3 4')" = "1" ]; then pass "a struct local keeps its type across lines"; else
    fail "a struct local keeps its type across lines" "$(echo "$out" | tr '\n' ' ')"; fi

echo ""
echo "=== quadrepl :doc ==="

out=$(repl ':doc dup\n')
if echo "$out" | grep -q '( a -- a a )'; then pass ":doc shows a builtin's stack effect"; else
    fail ":doc shows a builtin's stack effect" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl ':doc defer\n')
if echo "$out" | grep -q 'keyword'; then pass ":doc describes a keyword"; else
    fail ":doc describes a keyword" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl ':doc math::sqrt\n')
if echo "$out" | grep -q 'pub fn sqrt'; then pass ":doc reads a stdlib signature"; else
    fail ":doc reads a stdlib signature" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl ':doc trim\n')
if echo "$out" | grep -q 'strings::trim_left'; then pass ":doc searches when there is no exact match"; else
    fail ":doc searches when there is no exact match" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl 'fn double(x:i64 -- y:i64) { x x add }\n:doc double\n')
if echo "$out" | grep -q 'this session'; then pass ":doc finds a function defined in the session"; else
    fail ":doc finds a function defined in the session" "$(echo "$out" | tr '\n' ' ')"; fi

echo ""
echo "=== quadrepl standard library ==="

out=$(repl 'use math\n2.0 math::sqrt printv\n')
if echo "$out" | grep -q '1.41421'; then pass "a stdlib module links and runs"; else
    fail "a stdlib module links and runs" "$(echo "$out" | tr '\n' ' ')"; fi

out=$(repl 'use strings\n"  padded  " strings::trim prints\n')
if echo "$out" | grep -q '^padded$'; then pass "a second stdlib module links and runs"; else
    fail "a second stdlib module links and runs" "$(echo "$out" | tr '\n' ' ')"; fi

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
	"errcode=" print err -> code drop code print nl
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
echo "=== hostile input ==="

# Everything below is a regression for a crash, a wedge or a wrong exit code
# found by fuzzing the tools with the arguments and files a user never types on
# purpose but a script, an editor or a stray glob eventually will.

# A path longer than NAME_MAX reached the throwing std::filesystem overloads
# (is_directory / recursive_directory_iterator) and aborted with an uncaught
# filesystem_error -- no diagnostic, just SIGABRT and a core dump.
LONG_NAME=$(printf 'A%.0s' $(seq 1 100000))
for spec in "quadc:$QUADC" "quadfmt:$QUADFMT" "quadlint:$QUADLINT" "quaduses:$QUADUSES"; do
    name="${spec%%:*}"; bin="${spec#*:}"
    err=$("$bin" "$LONG_NAME" 2>&1 >/dev/null || true)
    rc=0; "$bin" "$LONG_NAME" >/dev/null 2>&1 || rc=$?
    if [ "$rc" -lt 128 ]; then pass "$name survives an over-long path"; else
        fail "$name survives an over-long path" "exit $rc"; fi
    if echo "$err" | grep -q "terminate called"; then
        fail "$name does not abort on an over-long path" "$(echo "$err" | head -1)"
    else pass "$name does not abort on an over-long path"; fi
done

# A directory tree holding something unreadable made the plain recursive
# iterator throw mid-walk. The tools must skip what they may not read.
mkdir -p "$WORK_DIR/hostile/readable" "$WORK_DIR/hostile/locked"
echo 'fn main() { 1 drop }' > "$WORK_DIR/hostile/readable/ok.qd"
echo 'fn other() { }' > "$WORK_DIR/hostile/locked/hidden.qd"
chmod 000 "$WORK_DIR/hostile/locked" 2>/dev/null || true
for spec in "quadfmt:$QUADFMT" "quadlint:$QUADLINT" "quaduses:$QUADUSES"; do
    name="${spec%%:*}"; bin="${spec#*:}"
    err=$("$bin" "$WORK_DIR/hostile" 2>&1 >/dev/null || true)
    rc=0; "$bin" "$WORK_DIR/hostile" >/dev/null 2>&1 || rc=$?
    if [ "$rc" -lt 128 ]; then pass "$name walks past an unreadable directory"; else
        fail "$name walks past an unreadable directory" "exit $rc"; fi
    if echo "$err" | grep -q "terminate called"; then
        fail "$name does not abort on an unreadable directory" "$(echo "$err" | head -1)"
    else pass "$name does not abort on an unreadable directory"; fi
done
chmod 755 "$WORK_DIR/hostile/locked" 2>/dev/null || true

# readFile() streamed a character device forever: `quadfmt /dev/zero` never
# returned. Only regular files are read now.
if [ -c /dev/zero ]; then
    for spec in "quadfmt:$QUADFMT" "quadlint:$QUADLINT" "quaduses:$QUADUSES"; do
        name="${spec%%:*}"; bin="${spec#*:}"
        if timeout 20 "$bin" /dev/zero >/dev/null 2>&1; rc=$?; [ "${rc:-0}" -ne 124 ]; then
            pass "$name does not hang on a character device"
        else
            fail "$name does not hang on a character device" "timed out"
        fi
    done
fi

# A directory passed where a file belongs must be reported, not read.
for spec in "quadfmt:$QUADFMT" "quaduses:$QUADUSES"; do
    name="${spec%%:*}"; bin="${spec#*:}"
    rc=0; "$bin" "$WORK_DIR/hostile/readable/" >/dev/null 2>&1 || rc=$?
    if [ "$rc" -lt 128 ]; then pass "$name handles a directory argument"; else
        fail "$name handles a directory argument" "exit $rc"; fi
done

# A file the scanner cannot read used to parse as a clean empty program, so
# quadlint printed "error: Invalid UTF-8" and still exited 0 -- a CI check over
# a binary file passed silently.
printf 'fn main() { 1 drop }\n\xff\xfe not utf8\n' > "$WORK_DIR/badutf8.qd"
for spec in "quadc:$QUADC" "quadfmt:$QUADFMT" "quadlint:$QUADLINT" "quaduses:$QUADUSES"; do
    name="${spec%%:*}"; bin="${spec#*:}"
    rc=0; "$bin" "$WORK_DIR/badutf8.qd" >/dev/null 2>&1 || rc=$?
    if [ "$rc" -eq 1 ]; then pass "$name exits 1 on invalid UTF-8"; else
        fail "$name exits 1 on invalid UTF-8" "exit $rc"; fi
done

# Reporting a diagnostic recomputed line/column by scanning the file from byte
# zero, so a file that is nothing but errors cost O(n^2) to reject: 20k stray
# braces took about nine seconds. The precomputed source maps make it linear.
printf '}%.0s' $(seq 1 20000) > "$WORK_DIR/allerrors.qd"
for spec in "quadfmt:$QUADFMT" "quadlint:$QUADLINT" "quaduses:$QUADUSES"; do
    name="${spec%%:*}"; bin="${spec#*:}"
    if timeout 20 "$bin" "$WORK_DIR/allerrors.qd" >/dev/null 2>&1; rc=$?; [ "${rc:-0}" -ne 124 ]; then
        pass "$name reports a file of pure errors promptly"
    else
        fail "$name reports a file of pure errors promptly" "timed out (quadratic diagnostics?)"
    fi
done

# Deep nesting must not run the parser out of stack.
{ printf 'fn main() {'; printf '{%.0s' $(seq 1 50000); printf '}%.0s' $(seq 1 50000); printf '}\n'; } \
    > "$WORK_DIR/deep.qd"
rc=0; timeout 60 "$QUADFMT" "$WORK_DIR/deep.qd" >/dev/null 2>&1 || rc=$?
if [ "$rc" -lt 128 ]; then pass "quadfmt survives 50k levels of nesting"; else
    fail "quadfmt survives 50k levels of nesting" "exit $rc"; fi

# Arguments made of invisible or control characters must be reported like any
# other missing file, never crash and never be executed by a shell.
for payload in "$(printf '\xe2\x80\x8b')" "$(printf '\xef\xbb\xbf')" "$(printf '\033[2J')" "-" "--" "; touch $WORK_DIR/pwned; #"; do
    rc=0; "$QUADLINT" "$payload" >/dev/null 2>&1 || rc=$?
    if [ "$rc" -lt 128 ]; then pass "quadlint survives an invisible/control argument"; else
        fail "quadlint survives an invisible/control argument" "exit $rc"; fi
done
if [ -e "$WORK_DIR/pwned" ]; then
    fail "arguments are not run through a shell" "a shell metacharacter argument created a file"
else
    pass "arguments are not run through a shell"
fi

# An output path that cannot be written must say which path and name the tool,
# rather than printing a bare strerror line with no context.
err=$("$QUADC" -o /dev/null/nope "$WORK_DIR/hostile/readable/ok.qd" 2>&1 || true)
if echo "$err" | grep -q "quadc: cannot write output '/dev/null/nope'"; then
    pass "quadc names the file it could not open"
else
    fail "quadc names the file it could not open" "$(echo "$err" | head -1)"
fi

# quadrepl reports its stack with agreeing grammar.
out=$(printf '42\nstack\n' | "$QUADREPL" 2>&1 || true)
if echo "$out" | grep -q "Stack (1 item)"; then pass "quadrepl says '1 item', not '1 items'"; else
    fail "quadrepl says '1 item', not '1 items'" "$(echo "$out" | grep -i item | head -1)"; fi
out=$(printf '1 2\nstack\n' | "$QUADREPL" 2>&1 || true)
if echo "$out" | grep -q "Stack (2 items)"; then pass "quadrepl still pluralises above one"; else
    fail "quadrepl still pluralises above one" "$(echo "$out" | grep -i item | head -1)"; fi

echo ""
if [ "$TESTS_FAILED" -gt 0 ]; then
    echo -e "${RED}FAILED${NC}: $TESTS_FAILED of $TESTS_RUN checks failed"
    exit 1
fi
echo -e "${GREEN}PASSED${NC}: all $TESTS_RUN checks passed"
exit 0
