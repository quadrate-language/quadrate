#!/bin/bash

# Peak-memory regressions: a program that builds and drops the same structure over
# and over must not grow.
#
# Structs are refcounted, and three defects meant the references were never given
# back: `>>field` overwrote a pointer field without releasing what it replaced,
# `==`/`!=` released their operands only when both were strings, and a caller of a
# struct-returning function was compiled as integer-only, which skips the release on
# every rebinding. None of the three shows up in output, in the stack depth, or in a
# short run -- only in memory that climbs with the iteration count. So the check is:
# run the same program at two very different iteration counts and compare peak RSS.
#
# Usage: run_memory_test.sh

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
QUADC="${QUADC:-$PROJECT_ROOT/$BUILD_DIR/cmd/quadc/quadc}"
export QUADRATE_ROOT="${QUADRATE_ROOT:-$PROJECT_ROOT/dist/share/quadrate}"
export QUADRATE_LIBDIR="${QUADRATE_LIBDIR:-$PROJECT_ROOT/dist/lib}"
export LD_LIBRARY_PATH="$PROJECT_ROOT/dist/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if [[ ! -x "$QUADC" ]]; then
	echo "SKIP: quadc not found at $QUADC"
	exit 0
fi
if ! command -v python3 >/dev/null 2>&1; then
	echo "SKIP: python3 not found (needed to read peak RSS)"
	exit 0
fi

# Peak RSS of a child process, in KB. argv[1] is where the child's stderr goes.
# A child that fails prints "FAILED <code>" rather than raising: this runs under
# `set -e`, where a non-zero exit inside a command substitution kills the script
# before it can say which program died, which is how a crash on CI arrives as a
# bare "test failed" with nothing to go on.
cat > "$WORK_DIR/peak_rss.py" <<'PY'
import os, resource, subprocess, sys

# VmHWM is the kernel's own high-water mark for the process, and exec starts it
# over, so where /proc has it that is what to read. getrusage(RUSAGE_CHILDREN) is
# the fallback and a poor one: a child forked from this interpreter starts out
# holding every page the interpreter holds, some 13 MB of it, and reports that as
# the peak. Under it sat a floor that no leak smaller than the interpreter could
# be seen over -- a struct's array field leaked 5.7 MB across a case here and the
# reading did not move. Poll instead; the mark only ever climbs, and the cmdline
# check keeps the readings taken before exec, which are this interpreter's, out
# of the maximum.
def poll_hwm(proc, program):
	want = os.path.basename(program).encode()
	status = "/proc/%d/status" % proc.pid
	cmdline = "/proc/%d/cmdline" % proc.pid
	hwm = 0
	while proc.poll() is None:
		try:
			with open(cmdline, "rb") as f:
				if os.path.basename(f.read().split(b"\0")[0]) != want:
					continue
			with open(status) as f:
				for line in f:
					if line.startswith("VmHWM:"):
						hwm = max(hwm, int(line.split()[1]))
						break
		except OSError:
			break
	return hwm

err_path = sys.argv[1]
argv = sys.argv[2:]
with open(err_path, "wb") as err:
	proc = subprocess.Popen(argv, stdout=subprocess.DEVNULL, stderr=err)
	hwm = poll_hwm(proc, argv[0]) if os.path.isdir("/proc") else 0
	returncode = proc.wait()

if returncode != 0:
	print("FAILED %d" % returncode)
	sys.exit(0)

if hwm:
	print(hwm)
else:
	peak = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
	# ru_maxrss is kilobytes on Linux and bytes on macOS.
	if sys.platform == "darwin":
		peak //= 1024
	print(peak)
PY

PASSED=0
FAILED=0

# $1 = name, $2 = source with @N@ where the iteration count goes, $3 = iterations for
# the long run, $4 = allowed growth in KB. The long run is sized so that the leak this
# case used to have would blow the budget several times over.
check_flat() {
	local name="$1" template="$2" high="$3" budget="$4"
	local low=200
	local rss_low rss_high growth

	for n in "$low" "$high"; do
		printf '%s\n' "${template//@N@/$n}" > "$WORK_DIR/${name}_$n.qd"
		if ! "$QUADC" -O2 -o "$WORK_DIR/${name}_$n" "$WORK_DIR/${name}_$n.qd" > "$WORK_DIR/${name}_$n.log" 2>&1; then
			echo -e "  ${RED}✗${NC} $name (failed to compile)"
			cat "$WORK_DIR/${name}_$n.log"
			FAILED=$((FAILED + 1))
			return
		fi
	done

	for n in "$low" "$high"; do
		local measured
		measured=$(python3 "$WORK_DIR/peak_rss.py" "$WORK_DIR/${name}_$n.err" "$WORK_DIR/${name}_$n")
		if [[ "$measured" == FAILED* ]]; then
			local why
			why=$(tr '\n' ' ' < "$WORK_DIR/${name}_$n.err" | cut -c1-300)
			echo -e "  ${RED}✗${NC} $name (the $n-iteration run exited ${measured#FAILED }: ${why:-no output on stderr})"
			FAILED=$((FAILED + 1))
			return
		fi
		if [[ "$n" == "$low" ]]; then
			rss_low=$measured
		else
			rss_high=$measured
		fi
	done
	growth=$((rss_high - rss_low))

	# Allocator behaviour is not the same everywhere -- how much of a freed block is
	# held on to, and how much of the heap is touched at all, differ between glibc and
	# musl and between versions of each. So the budget is the stated one or a quarter
	# of what the short run already used, whichever is larger; the leaks these cases
	# were written for ran to hundreds of megabytes, which neither figure comes near.
	local allowance=$((rss_low / 4))
	if [[ $allowance -lt $budget ]]; then
		allowance=$budget
	fi

	if [[ $growth -gt $allowance ]]; then
		echo -e "  ${RED}✗${NC} $name (peak RSS grew ${growth} KB from $low to $high iterations: ${rss_low} KB -> ${rss_high} KB, allowed ${allowance} KB)"
		FAILED=$((FAILED + 1))
	else
		echo -e "  ${GREEN}✓${NC} $name (${growth} KB over 100x the work: ${rss_low} KB -> ${rss_high} KB)"
		PASSED=$((PASSED + 1))
	fi
}

echo "Memory tests"

# A parent with two children, linked through a tail pointer that moves on: the shape
# every list-building word has, and the one that leaked every node.
check_flat "struct_graph" 'struct V { head:*V tail:*V next:*V count:i64 }

fn mk( -- v:V) {
	V { head = null tail = null next = null count = 0 }
}

fn (parent:V) link(child:V -- p:V) {
	parent <<tail -> t
	t null == if {
		parent child >>head drop
	} else {
		t child >>next drop
	}
	parent child >>tail drop
	parent parent <<count 1 + >>count drop
	parent
}

fn build( -- v:V) {
	mk -> items
	0 3 1 for i {
		mk -> elem
		items elem link -> items
	}
	items
}

fn main(--) {
	0 -> acc
	0 @N@ 1 for i {
		build <<count acc + -> acc
	}
	acc print nl
}' 50000 4096

# A struct returned by a call and bound in a loop, in a body that is otherwise all
# integers -- the shape the integer-only fast paths mis-compiled. Nothing here reads
# the struct on purpose: a field access is one of the things the body scan already
# rejects, so touching `v` would put the function back on the checked paths and the
# case would prove nothing.
check_flat "returned_struct" 'struct A { x:i64 }

fn mkx( -- v:A) {
	A { x = 1 }
}

fn main(--) {
	0 -> acc
	0 @N@ 1 for i {
		mkx -> v
		acc 1 + -> acc
	}
	acc print nl
}' 400000 4096

# The whole json parser, over a document with arrays, objects and strings in it.
check_flat "json_parse" 'use json

fn main(--) {
	0 -> acc
	0 @N@ 1 for i {
		"{\"a\":[1,2,3],\"b\":{\"c\":\"x\",\"d\":true},\"e\":null}" json::parse! -> doc
		doc json::len acc + -> acc
	}
	acc print nl
}' 30000 4096

# Containers hold a reference to every string and struct in them, and give it back when the
# element is popped, replaced, reset or released. Nothing about that shows in their output.
check_flat "containers" 'use ct
use strconv

struct Item {
	n:     i64
	label: str
}

fn round( -- n:i64) {
	Vec<str> { data = null len = 0 cap = 0 } -> v
	Vec<Item> { data = null len = 0 cap = 0 } -> vi
	Map<str> { keys = null values = null states = null len = 0 cap = 0 } -> m
	0 20 1 for i {
		v i strconv::itoa push! -> v
		vi Item { n = i label = "x" } push! -> vi
		m i strconv::itoa i strconv::itoa insert! -> m
	}
	v 0 "replaced" set! -> v
	v pop! -> v -> _
	m "0" remove! -> m
	v length vi length + m length +
	v release
	vi release
	m release
}

fn main(--) {
	0 -> acc
	0 @N@ 1 for i {
		round acc + -> acc
	}
	acc print nl
}' 20000 4096

# The places that hold a reference, one program each: an array, a closure's captured block, a
# deferred block, a generic struct field and a plain string field overwritten in a loop.
# Specification 11.2.2 states the contract they share; this is what checks they all keep it.
check_flat "holders" 'use strings
use strconv

struct Box<T> {
	value: T
}

struct Holder {
	label: str
}

struct P {
	n: i64
}

fn make_adder(n:i64 -- f:fn(i64 -- i64)) {
	fn (x:i64 -- r:i64) { x n + }
}

fn round( -- n:i64) {
	// an array of strings, and one of structs
	["alpha" "beta"] -> words
	[] -> items
	items P { n = 1 } append -> items
	words 0 nth strings::len items 0 nth <<n + -> total

	// a closure over a value, called and dropped
	5 make_adder -> add5
	3 add5 call total + -> total

	// a deferred block holding a string
	"deferred" -> d
	defer { d strings::len drop }

	// a generic field, overwritten
	Box<str> { value = "first" } -> b
	b "second" >>value drop
	b <<value strings::len total + -> total

	// a plain string field, overwritten in a loop
	Holder { label = "start" } -> h
	0 5 1 for i {
		h i strconv::itoa "-tail" strings::concat >>label drop
	}
	h <<label strings::len total +
}

fn main(--) {
	0 -> acc
	0 @N@ 1 for i {
		round acc + -> acc
	}
	acc print nl
}' 20000 4096

# A struct field holding an array. The field owns a reference the same way a str field
# does -- construction moves the one the stack held into the slot, and `>>cells` hands
# back the one it replaced -- but the generated destructor released only the strings and
# the nested structs, so the qd_array_t and its buffer outlived every struct built in a
# loop. Nothing lost it: the pointer registry still held both at exit, so valgrind called
# them reachable and the language suite saw a clean run. Only the heap climbing says so.
check_flat "struct_array_field" 'struct Row {
	label: str
	cells: []i64
}

fn make( -- r:Row) {
	Row { label = "row" cells = [1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16] }
}

fn round( -- n:i64) {
	make -> r
	r <<cells 1 nth -> total
	r [17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32] >>cells drop
	r <<cells len total +
}

fn main(--) {
	0 -> acc
	0 @N@ 1 for i {
		round acc + -> acc
	}
	acc print nl
}' 30000 4096

# A module-level `var` is one of the places that holds a reference, and the store
# path never gave the old one back: `-> g` wrote the new pointer straight over the
# old one. Valgrind cannot see this for a struct -- the pointer registry still
# holds it at exit, so it reads as reachable -- which is why it is measured here.
check_flat "global_var_store" 'struct Point {
	x: i64
	y: i64
}

var origin = Point { x = 0 y = 0 }
var label:str = "start"

fn round(i:i64 -- n:i64) {
	Point { x = i y = i } -> origin
	"round" -> label
	origin <<x
}

fn main(--) {
	0 -> acc
	0 @N@ 1 for i {
		i round acc + -> acc
	}
	acc print nl
	label print nl
}' 30000 4096

echo ""
echo "Memory tests: $PASSED passed, $FAILED failed"
[[ $FAILED -eq 0 ]]
