#!/bin/bash

# Regression tests for quadc command-line behaviour: output paths, -r program
# execution, input errors and link-time library names

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

QUADC="${QUADC:-build/debug/cmd/quadc/quadc}"
if [[ "$QUADC" != /* ]]; then
    QUADC="$(pwd)/$QUADC"
fi

TEST_DIR="$(mktemp -d /tmp/quadc_cli_test_XXXXXXXX)"
PWN_MARKER="$TEST_DIR/pwned"

cleanup() {
    chmod -R u+rwx "$TEST_DIR" 2>/dev/null
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

pass() {
    echo -e "  ${GREEN}✓${NC} $1"
    ((TESTS_PASSED++))
    ((TESTS_RUN++))
}

fail() {
    echo -e "  ${RED}✗${NC} $1"
    if [[ -n "${2:-}" ]]; then
        echo "    $2"
    fi
    ((TESTS_FAILED++))
    ((TESTS_RUN++))
}

mkdir -p "$TEST_DIR/cache" "$TEST_DIR/tmp" "$TEST_DIR/src"
export XDG_CACHE_HOME="$TEST_DIR/cache"
export QUADRATE_ROOT="$(realpath "${QUADRATE_ROOT:-dist/share/quadrate}")"
export QUADRATE_LIBDIR="$(realpath "${QUADRATE_LIBDIR:-dist/lib}")"
export NO_COLOR=1

if [ ! -x "$QUADC" ]; then
    echo -e "${RED}Error: quadc not found at $QUADC${NC}"
    exit 1
fi

cd "$TEST_DIR/src" || exit 1

cat > hello.qd << 'EOF'
fn main() {
    "Hello" print nl
}
EOF

cat > echo_args.qd << 'EOF'
use os

fn main() {
	os::args -> args
	args len -> argc
	"argc=" print argc print nl
	0 argc 1 for i {
		"arg" print i print "=[" print args i nth print "]" print nl
	}
	args free
}
EOF

cat > exit3.qd << 'EOF'
use os
fn main() {
    3 os::exit
}
EOF

# ===================================================================
# Output path must not overwrite an input file
# ===================================================================
cp hello.qd same.qd
output=$("$QUADC" -o same.qd same.qd 2>&1)
if [[ $? -ne 0 ]] && cmp -s hello.qd same.qd && echo "$output" | grep -q "same as input"; then
    pass "-o naming the input file is refused"
else
    fail "-o naming the input file must be refused" "$output"
fi

for name in main prog; do
    cp hello.qd "$name"
    output=$("$QUADC" "$name" 2>&1)
    if [[ $? -ne 0 ]] && cmp -s hello.qd "$name"; then
        pass "extensionless input '$name' is not clobbered by the default output"
    else
        fail "extensionless input '$name' was clobbered or accepted" "$output"
    fi
done

cp hello.qd script
actual=$("$QUADC" -r script 2>&1)
if [[ "$actual" == "Hello" ]] && cmp -s hello.qd script; then
    pass "-r still runs an extensionless source"
else
    fail "-r on an extensionless source failed" "$actual"
fi

cp hello.qd obj.o
output=$("$QUADC" --freestanding -o obj obj.o 2>&1)
if [[ $? -ne 0 ]] && cmp -s hello.qd obj.o; then
    pass "--freestanding object output does not clobber the input"
else
    fail "--freestanding object output clobbered the input" "$output"
fi

# ===================================================================
# The intermediate object file does not touch <output>.o
# ===================================================================
echo "precious" > keep.o
"$QUADC" --no-jit -o keep hello.qd > /dev/null 2>&1
if [[ "$(cat keep.o 2>/dev/null)" == "precious" ]]; then
    pass "existing <output>.o is left alone"
else
    fail "existing <output>.o was overwritten or deleted"
fi

# ===================================================================
# Missing output directory is an error with and without the cache
# ===================================================================
"$QUADC" -o cached hello.qd > /dev/null 2>&1
for attempt in 1 2; do
    output=$("$QUADC" -o nodir/x hello.qd 2>&1)
    if [[ $? -ne 0 ]] && [[ ! -e nodir ]]; then
        pass "-o into a missing directory fails (attempt $attempt)"
    else
        fail "-o into a missing directory must fail (attempt $attempt)" "$output"
    fi
done

output=$("$QUADC" -o hello.qd/x hello.qd 2>&1)
if [[ $? -ne 0 ]] && echo "$output" | grep -q "is not a directory" && cmp -s hello.qd same.qd; then
    pass "-o under a regular file fails and names the reason"
else
    fail "-o under a regular file must fail" "$output"
fi

# ===================================================================
# Input file errors
# ===================================================================
output=$("$QUADC" -o out missing.qd 2>&1)
if [[ $? -ne 0 ]] && echo "$output" | grep -q "No such file" && ! echo "$output" | grep -q "no main module"; then
    pass "missing input file is an error"
else
    fail "missing input file must be an error" "$output"
fi

if [[ $(id -u) -ne 0 ]]; then
    cp hello.qd unreadable.qd
    chmod 000 unreadable.qd
    output=$("$QUADC" -o out unreadable.qd 2>&1)
    if [[ $? -ne 0 ]] && echo "$output" | grep -q "Permission denied"; then
        pass "unreadable input reports the real error"
    else
        fail "unreadable input must report permission denied" "$output"
    fi
    chmod 644 unreadable.qd
fi

mkdir -p other
cp hello.qd first.qd
cp hello.qd other/second.qd
output=$("$QUADC" -o out first.qd other/second.qd 2>&1)
if [[ $? -ne 0 ]] && echo "$output" | grep -q "multiple input files define 'main'"; then
    pass "two input files defining main are rejected"
else
    fail "two input files defining main must be rejected" "$output"
fi

# ===================================================================
# -r passes arguments verbatim and reports the exit status
# ===================================================================
expected='argc=3
arg0=[]
arg1=[*]
arg2=[a;b $HOME]'
for attempt in uncached cached; do
    actual=$("$QUADC" -r --no-jit echo_args.qd -- '' '*' 'a;b $HOME' 2>/dev/null)
    if [[ "$actual" == "$expected" ]]; then
        pass "-r passes empty, glob and shell-special arguments verbatim ($attempt)"
    else
        fail "-r mangled program arguments ($attempt)" "got: $actual"
    fi
done

for attempt in uncached cached; do
    "$QUADC" -r --no-jit exit3.qd > /dev/null 2>&1
    code=$?
    if [[ $code -eq 3 ]]; then
        pass "-r propagates the program exit code ($attempt)"
    else
        fail "-r must propagate exit code 3 ($attempt)" "got $code"
    fi
done

mkdir -p "$TEST_DIR/tmp with space"
actual=$(TMPDIR="$TEST_DIR/tmp with space" "$QUADC" -r --no-jit -o 'my prog' echo_args.qd -- x 2>&1)
if [[ "$actual" == "argc=1
arg0=[x]" ]]; then
    pass "-r works with spaces in TMPDIR and output name"
else
    fail "-r failed with spaces in TMPDIR or output name" "$actual"
fi
if [[ -z "$(ls -A "$TEST_DIR/tmp with space")" ]]; then
    pass "-r cleans up its temporary directory"
else
    fail "-r left files in TMPDIR" "$(ls -A "$TEST_DIR/tmp with space")"
fi

# ===================================================================
# Library names from imports are not interpreted by a shell
# ===================================================================
cat > inject.qd << EOF
import "libm;touch \${IFS}$PWN_MARKER;.so" as "cm" {
    fn cos(x:f64 -- r:f64)
}
fn main() {
    1.0 cm::cos print nl
}
EOF
output=$("$QUADC" -o inject inject.qd 2>&1)
code=$?
if [[ ! -e "$PWN_MARKER" ]] && [[ $code -ne 0 ]]; then
    pass "shell metacharacters in an import library name are not executed"
else
    fail "import library name was passed through a shell" "$output"
fi

cat > libm_import.qd << 'EOF'
import "libm.so" as "cm" {
    fn cos(x:f64 -- r:f64)
}
fn main() {
    0.0 cm::cos print nl
}
EOF
actual=$("$QUADC" -o libm_import libm_import.qd 2>&1)
if [[ $? -eq 0 && -x libm_import ]]; then
    pass "a plain .so import still links"
else
    fail "a plain .so import failed to link" "$actual"
fi

# ===================================================================
# A freestanding program links against the freestanding runtime
# ===================================================================
# Nothing else builds one: the runtime stopped compiling when the hosted context gained a
# jmp_buf, and stack code needed words it never had, unnoticed until the kernel example was
# built. Stack words, a local, and a loop, linked with what the runtime defines, leave nothing
# undefined.
if command -v clang > /dev/null 2>&1 && command -v ld > /dev/null 2>&1; then
    printf 'fn _start() {\n\t1 2 3 rot drop2 dup over swap nip 4 pick drop2 tuck drop2 drop\n\t7 -> x\n\tx drop\n\tloop { }\n}\n' > bare.qd
    if ! output=$("$QUADC" --freestanding --target x86_64-unknown-none-elf bare.qd -o bare.o 2>&1); then
        fail "a freestanding program did not compile" "$output"
    elif ! output=$(clang --target=x86_64-unknown-none-elf -ffreestanding -fno-builtin -nostdlib \
            -fno-stack-protector -mno-red-zone -O2 -I"$REPO_ROOT/lib/rt/include" \
            -c "$REPO_ROOT/lib/rt/src/freestanding.c" -o rt.o 2>&1); then
        fail "the freestanding runtime did not compile" "$output"
    elif ! output=$(ld -r -o linked.o bare.o rt.o 2>&1); then
        fail "a freestanding program did not link" "$output"
    elif [[ -n "$(nm -u linked.o)" ]]; then
        fail "a freestanding program left symbols undefined" "$(nm -u linked.o | tr '\n' ' ')"
    else
        pass "a freestanding program links against the freestanding runtime"
    fi
fi

# A call to another module's word keeps the caller's native version when the word has one.
# Whether it did depended on declaration order: the program is declared before its modules, so
# `bits::mask` had no native version yet when `add32` was looked at, and sha256, built of such
# words, ran entirely on the runtime stack.
printf 'use bits\nfn add32(a:i64 b:i64 -- r:i64) { + 32 bits::mask }\nfn main() { 1 2 add32 print nl }\n' > native_across.qd
if ! output=$("$QUADC" --dump-ir native_across.qd -o native_across 2>&1); then
    fail "a call to bits::mask did not compile" "$output"
elif ! grep -q 'add32_native' <<< "$output"; then
    fail "a function calling bits::mask lost its native version"
else
    pass "a function calling bits::mask keeps its native version"
fi

# A word of several results has a native version, and so does a word that calls one
printf 'fn divmod(a:i64 b:i64 -- q:i64 r:i64) { dup2 / rrot %% }\nfn digits(n:i64 -- s:i64) { 0 swap loop { dup 0 == if { break } 10 divmod rot + swap } drop }\nfn main() { 123 digits print nl }\n' > several_results.qd
if ! output=$("$QUADC" --dump-ir several_results.qd -o several_results 2>&1); then
    fail "a word of several results did not compile" "$output"
elif ! grep -q 'divmod_native' <<< "$output" || ! grep -q 'digits_native' <<< "$output"; then
    fail "a word of several results, or its caller, has no native version"
else
    pass "a word of several results and its caller have native versions"
fi

# A function taking a ptr among numbers has a native version; one that prints the ptr has not
printf 'use mem\ndef word(buf:ptr i:i64 -- v:i64) { buf i 8 * mem::get_i64 }\ndef shown(buf:ptr -- ) { buf print nl }\nfn main() { 16 mem::alloc! -> b  7 b 0 mem::set_i64  b 0 word print nl  b mem::free }\n' > native_ptr.qd
if ! output=$("$QUADC" --dump-ir native_ptr.qd -o native_ptr 2>&1); then
    fail "a function taking a ptr did not compile" "$output"
elif ! grep -q 'word_native' <<< "$output"; then
    fail "a function taking a ptr has no native version"
elif grep -q 'shown_native' <<< "$output"; then
    fail "a function printing a ptr has a native version, which would print it as an integer"
else
    pass "a function taking a ptr has a native version, unless it prints it"
fi

# A function reading a str by len and nth has a native version; one comparing strings has not
printf 'fn lines(s:str -- n:i64) { 0 swap fn { 10 == + } each }\ndef same(a:str b:str -- yes:i64) { a b == }\nfn main() { "a\\nb" lines print "x" "x" same print nl }\n' > native_str.qd
if ! output=$("$QUADC" --dump-ir native_str.qd -o native_str 2>&1); then
    fail "a function taking a str did not compile" "$output"
elif ! grep -q 'lines_native' <<< "$output"; then
    fail "a function reading a str by len and nth has no native version"
elif grep -q 'same_native' <<< "$output"; then
    fail "a function comparing strings has a native version, which would compare addresses"
else
    pass "a function reading a str has a native version, unless it uses it as a string"
fi

# A word with no inputs has a native version; one reading a str global or constant has not
printf 'var count = 0\nvar name:str = "x"\nconst Greeting = "hi"\nfn answer( -- n:i64) { 42 }\nfn tick( -- ) { count 1 + -> count }\nfn shown( -- ) { name print nl }\nfn greet( -- ) { Greeting print nl }\nfn main() { answer print tick shown greet }\n' > native_noinputs.qd
if ! output=$("$QUADC" --dump-ir native_noinputs.qd -o native_noinputs 2>&1); then
    fail "a word with no inputs did not compile" "$output"
elif ! grep -q 'answer_native' <<< "$output" || ! grep -q 'tick_native' <<< "$output"; then
    fail "a word with no inputs has no native version"
elif grep -q 'shown_native' <<< "$output" || grep -q 'greet_native' <<< "$output"; then
    fail "a word reading a str global or constant has a native version, which would read it as a number"
else
    pass "a word with no inputs has a native version, unless it reads a str global or constant"
fi

# ===================================================================
# Summary
# ===================================================================
echo ""
echo "================================"
echo -e "Results: ${GREEN}$TESTS_PASSED passed${NC}, ${RED}$TESTS_FAILED failed${NC} (out of $TESTS_RUN)"
echo "================================"

if [[ $TESTS_FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
