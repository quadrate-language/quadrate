#!/usr/bin/env python3
"""Fail if a fallible function aborts instead of propagating.

`f!` at a call site aborts the whole process on error; `f?` returns the error to
the caller. Inside a function that is itself declared fallible (`)!`), the abort
form throws away the one thing the signature promised the caller -- the chance to
handle it. The stdlib had 70 such sites and zero uses of `?`; they were converted
in one pass, and this keeps them converted.

Functions that are *not* declared fallible are deliberately not checked. There are
159 abort sites in those, and each is a real decision (declare the function
fallible, or establish that dying is correct) rather than a mechanical rewrite.

Usage: tools/check_fallible_propagation.py [paths...]   (default: lib)
"""

import os
import re
import sys

# A call-site '!': identifier, optionally module-qualified, followed by '!' that
# is not '!='. The lookbehind keeps '>>field!' and '<<' out of it.
CALL = re.compile(r'(?<![A-Za-z0-9_>])([a-z_][A-Za-z0-9_]*(?:::[a-z_][A-Za-z0-9_]*)?)!(?!=)')

# fn f(..), pub fn f(..), receivers `fn (self:T) m(..)`, generic methods
# `fn (q:Queue<T>) enqueue<T>(..)`. The trailing group is the fallible marker.
FN = re.compile(
    r'^\s*(?:pub\s+)?fn\s*(?:\([^)]*\)\s*)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:<[^>]*>)?\s*\((.*)\)\s*(!?)\s*\{?\s*$')


def strip_noise(line):
    """Drop line comments and string bodies so their contents never match."""
    line = re.sub(r'//.*$', '', line)
    return re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)


def scan(path):
    problems = []
    with open(path, encoding='utf-8') as handle:
        current, fallible = None, False
        for lineno, raw in enumerate(handle, 1):
            match = FN.match(raw)
            if match:
                current, fallible = match.group(1), match.group(3) == '!'
            if not fallible:
                continue
            for call in CALL.finditer(strip_noise(raw)):
                problems.append((path, lineno, current, call.group(1)))
    return problems


def main():
    roots = sys.argv[1:] or ['lib']
    problems = []
    for root in roots:
        for dirpath, _, filenames in os.walk(root):
            for name in filenames:
                if name.endswith('.qd'):
                    problems += scan(os.path.join(dirpath, name))

    if problems:
        for path, lineno, fn, call in problems:
            print(f"error: {path}:{lineno}: fallible fn '{fn}' aborts on '{call}!'; "
                  f"use '{call}?' to propagate", file=sys.stderr)
        print(f"\n{len(problems)} abort(s) inside fallible functions", file=sys.stderr)
        return 1

    print("no fallible function aborts instead of propagating")
    return 0


if __name__ == '__main__':
    sys.exit(main())
