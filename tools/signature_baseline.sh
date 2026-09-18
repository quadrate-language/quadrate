#!/bin/bash
#
# Dump every function signature the semantic validator computes, across the whole corpus.
#
# Five separate builders populate mFunctionSignatures -- main-module functions, main-module
# import blocks, module functions, module import blocks, plus the struct-construction field
# evaluator -- each with its own mapping from a declared type name to a stack type and its own
# struct-qualification rules. They have drifted apart repeatedly. This captures what they all
# currently produce so a consolidation can be proved behaviour-preserving: take a baseline,
# refactor, diff.
#
# It is also a divergence detector on its own: a function whose signature is computed by two
# different builders shows up as two lines with the same name and different bodies.
#
# Usage:
#     tools/signature_baseline.sh <output-file> [quadc]
#
set -uo pipefail

OUT="${1:?usage: signature_baseline.sh <output-file> [quadc]}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QUADC="${2:-$ROOT/dist/bin/quadc}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

raw="$WORK/raw"
: > "$raw"
units=0
failed=0

# quadc validates the program, then each module file it pulled in, as separate passes. Only the
# first is the compile that produces code and sees the program's own view of every module; later
# passes validate a module *as its own main file*, where its structs are correctly unqualified.
# Comparing across passes compares different contexts, so take pass 1 only.
capture() { # capture <dir> <file> [extra quadc args...]
    local dir="$1" file="$2"; shift 2
    (cd "$dir" && QUADC_DUMP_SIGNATURES=1 "$QUADC" "$file" -o "$WORK/out.bin" "$@" 2>&1 >/dev/null) \
        | awk '/^PASS /{p=$2} /^SIG /{ if (p=="1") print }' >> "$raw"
    units=$((units + 1))
}

# Every standard library module, through a stub that imports it. This is the module-function
# and module-import-block builders.
for d in "$ROOT"/stdlib/*/qd/*/; do
    m="$(basename "$d")"
    case "$m" in rt|sys) continue;; esac
    printf 'use %s\nfn main() { }\n' "$m" > "$WORK/use_$m.qd"
    capture "$WORK" "use_$m.qd"
done

# Examples and tests: main-module functions, main-module import blocks, and the test modules
# under tests/qd/*/ which exercise module loading from a sibling directory.
while IFS= read -r f; do
    case "$f" in *"/kernel/"*) continue;; esac
    grep -q '^fn main\|^pub fn main' "$f" || continue
    capture "$(dirname "$f")" "$(basename "$f")"
done < <(find "$ROOT/examples" "$ROOT/tests/qd" -name '*.qd' | sort)

sort -u "$raw" > "$OUT"
echo "signature_baseline: $units compilation units, $(wc -l < "$OUT") distinct signatures -> $OUT"

# A name with more than one distinct signature was computed by two builders that disagree.
awk '{ name=$2; line=$0; if (name in seen && seen[name] != line) dup[name]=1; seen[name]=line }
     END { n=0; for (k in dup) n++; if (n) print "signature_baseline: " n " name(s) computed inconsistently (see --diverged)" }' "$OUT"
