#!/usr/bin/env python3
"""Compile (and optionally run) the Quadrate code blocks embedded in the docs.

Doc examples drift: they are written by hand, never executed, and rot silently whenever the
language changes underneath them. Three separate rounds of "the doc example does not compile"
have been fixed by hand; this makes it mechanical.

Scope: fenced ```quadrate / ```qd blocks that are complete programs (they define `fn main`).
Those are unambiguously runnable. Bare fragments -- `point <<x`, a lone signature, a snippet
that references variables from surrounding prose -- are not checked, because there is no sound
way to synthesise the context they assume. That is the large majority of blocks, so this gate
is a floor, not a ceiling.

Markers, written either as a `//` comment on any line of the block, or -- to keep them out of
the rendered page -- as an HTML comment on the line immediately before the opening fence:

    // doccheck: skip <reason>     do not check this block at all
    // doccheck: compile-only      compile it, but do not run it
    // doccheck: expect-error      compilation MUST fail (for examples that demonstrate a
                                   diagnostic); the check fails if it compiles cleanly

    <!-- doccheck: skip fragment shown piece by piece in the prose -->
    ```qd
    fn main() {
    ```

Always give `skip` a reason: it is the difference between "this example is prose" and "this
example is broken and someone silenced it".

Usage:
    tools/check_docs.py [--quad PATH] [--run] [--verbose] [PATHS...]

Exit status is non-zero if any checked block fails.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

FENCE = re.compile(r"^([ \t]*)```(quadrate|qd)[ \t]*$(.*?)^[ \t]*```[ \t]*$", re.S | re.M)
MARKER = re.compile(r"//\s*doccheck:\s*(skip|compile-only|expect-error)\b[ \t]*(.*)")
# Same marker in an HTML comment just above the fence, so it stays out of the rendered page.
PRE_MARKER = re.compile(r"<!--\s*doccheck:\s*(skip|compile-only|expect-error)\b[ \t]*(.*?)-->\s*$")


def find_blocks(path):
    """Yield (line_number, body, marker, marker_arg) for each fenced Quadrate block."""
    src = open(path, encoding="utf-8").read()
    for m in FENCE.finditer(src):
        indent, body = m.group(1), m.group(3)
        if indent:  # strip the common indent of a nested fence (e.g. inside a list item)
            body = "".join(
                line[len(indent):] if line.startswith(indent) else line
                for line in body.splitlines(keepends=True)
            )
        line_no = src.count("\n", 0, m.start()) + 1
        mk = MARKER.search(body)
        if not mk:
            # look at the non-blank line immediately preceding the fence
            before = src[: m.start()].rstrip("\n").rsplit("\n", 1)
            mk = PRE_MARKER.search(before[-1]) if before else None
        yield line_no, body.lstrip("\n"), (mk.group(1) if mk else None), (mk.group(2).strip() if mk else "")


def is_program(body):
    return re.search(r"^\s*(pub\s+)?fn\s+main\s*\(", body, re.M) is not None


def check(quad, path, line_no, body, marker, run, verbose):
    """Return (status, detail) where status is 'ok', 'skip', or 'fail'."""
    if marker == "skip":
        return "skip", ""
    if not is_program(body):
        return "skip", ""

    with tempfile.TemporaryDirectory(prefix="qddoc_") as td:
        src = os.path.join(td, "doc.qd")
        with open(src, "w", encoding="utf-8") as fh:
            fh.write(body)
        exe = os.path.join(td, "doc")

        try:
            cp = subprocess.run(
                [quad, "build", src, "-o", exe],
                capture_output=True, text=True, timeout=120,
            )
        except subprocess.TimeoutExpired:
            return "fail", "compile timed out after 120s"
        if marker == "expect-error":
            if cp.returncode == 0:
                return "fail", "expected a compile error, but it compiled cleanly"
            return "ok", ""
        if cp.returncode != 0:
            return "fail", (cp.stdout + cp.stderr).strip()

        if not run or marker == "compile-only":
            return "ok", ""

        try:
            # stdin is closed: an example that reads input should be marked compile-only rather
            # than blocking the run forever. cwd is the temp dir so an example that writes a
            # file (file-processing.md writes output.txt) cannot litter the working tree.
            rp = subprocess.run(
                [exe], capture_output=True, text=True, timeout=60,
                stdin=subprocess.DEVNULL, cwd=td,
            )
        except subprocess.TimeoutExpired:
            return "fail", "run timed out after 60s (mark it `compile-only` if it loops or waits for input)"
        if rp.returncode != 0:
            detail = (rp.stdout + rp.stderr).strip()
            return "fail", f"exited {rp.returncode}\n{detail}"
        if verbose and rp.stdout:
            return "ok", rp.stdout.strip()
        return "ok", ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="*", default=None)
    ap.add_argument("--quad", default="dist/bin/quad", help="path to the quad driver")
    ap.add_argument("--run", action="store_true", help="also execute each compiled program")
    ap.add_argument("--verbose", "-v", action="store_true")
    args = ap.parse_args()

    def expand(p):
        if os.path.isdir(p):
            out = []
            for root, _, files in os.walk(p):
                out.extend(os.path.join(root, f) for f in files if f.endswith(".md"))
            return out
        return [p]

    paths = [q for p in (args.paths or ["docs/docs"]) for q in expand(p)]
    paths.sort()

    quad = os.path.abspath(args.quad)
    if not os.path.exists(quad):
        sys.exit(f"check_docs: no quad driver at {quad} (run `make debug` first)")

    checked = skipped = failed = 0
    for path in paths:
        for line_no, body, marker, _arg in find_blocks(path):
            status, detail = check(quad, path, line_no, body, marker, args.run, args.verbose)
            if status == "skip":
                skipped += 1
                continue
            checked += 1
            if status == "fail":
                failed += 1
                print(f"{path}:{line_no}: FAIL")
                for line in detail.splitlines():
                    print(f"    {line}")
            elif args.verbose:
                print(f"{path}:{line_no}: ok" + (f"\n    {detail}" if detail else ""))

    verb = "checked" if not args.run else "compiled and ran"
    print(f"check_docs: {verb} {checked} block(s), {failed} failed, {skipped} skipped (not programs)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
