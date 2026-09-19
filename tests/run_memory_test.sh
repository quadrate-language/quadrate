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

# Peak RSS of a child process, in KB.
cat > "$WORK_DIR/peak_rss.py" <<'PY'
import resource, subprocess, sys
subprocess.run(sys.argv[1:], check=True, stdout=subprocess.DEVNULL)
print(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)
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

	rss_low=$(python3 "$WORK_DIR/peak_rss.py" "$WORK_DIR/${name}_$low")
	rss_high=$(python3 "$WORK_DIR/peak_rss.py" "$WORK_DIR/${name}_$high")
	growth=$((rss_high - rss_low))

	if [[ $growth -gt $budget ]]; then
		echo -e "  ${RED}✗${NC} $name (peak RSS grew ${growth} KB from $low to $high iterations, budget ${budget} KB)"
		FAILED=$((FAILED + 1))
	else
		echo -e "  ${GREEN}✓${NC} $name (${growth} KB over 100x the work)"
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

echo ""
echo "Memory tests: $PASSED passed, $FAILED failed"
[[ $FAILED -eq 0 ]]
