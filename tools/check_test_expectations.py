#!/usr/bin/env python3
"""Fail if a test under tests/qd asserts nothing.

A `.qd` file there normally pins its behaviour with a sibling `.out`/`.expected`
(stdout), `.err` (compile error) or `.runtime_err` (runtime error). A file with
none of those is skipped by the `qd` suite, and a forgotten `.out` looks exactly
like a helper module that was never meant to run on its own.

Most of them are the latter -- modules that exist to be imported by a sibling
test, which is where the assertion lives. This script proves that: a file with
no expectation of its own must be reached from one that has one, by
relative-path `use`, by its module (directory) name, or by a symbol it defines
being named in a file of the same module. Anything else is a test asserting
nothing, and is reported.

The exceptions are the files driven by a suite other than `qd`, listed in
EXEMPT below with the suite that asserts them.

Usage: tools/check_test_expectations.py [root]   (default: tests/qd)
"""

import os
import re
import sys
from fnmatch import fnmatch

PINNED_SUFFIXES = ('.out', '.expected', '.err', '.runtime_err')

# Files whose assertions live in another suite of tests/run_all.sh, so they
# legitimately have no expectation sibling here.
EXEMPT = (
    ('args/*.qd', 'driven by the args suite, which pins the output per argument list'),
    ('http/server_integration.qd', 'blocking server, driven by the http suite'),
    ('http/sse_integration.qd', 'blocking server, driven by the http suite'),
    ('stdlib/*_test.qd', 'test blocks asserted by the stdlib suite (quad test)'),
)

# `fn f(`, `pub fn f(`, `fn (self:T) m(`, `const C =`, `var v:`, `struct S {`,
# `packed struct S {`, `enum E {`. The name is what another file would call.
DEFINITION = re.compile(
    r'^\s*(?:pub\s+)?(?:fn\s*(?:\([^)]*\)\s*)?|const\s+|var\s+|(?:packed\s+)?struct\s+|enum\s+)'
    r'([A-Za-z_][A-Za-z0-9_]*)')


def strip_comments(text):
    """Drop line comments so a comment naming a helper is not a use of it.

    String bodies survive, because `use "nested/sky.qd"` is how a file in a
    subdirectory is imported.
    """
    return re.sub(r'//[^\n]*', '', text)


def strip_strings(text):
    """Also drop string bodies, for the matches that are symbol names.

    A printed `"Point"` is not a use of a struct called Point.
    """
    return re.sub(r'"(?:[^"\\\n]|\\.)*"', '""', text)


def is_pinned(path):
    stem = path[: -len('.qd')]
    return any(os.path.exists(stem + suffix) for suffix in PINNED_SUFFIXES)


def exemption(relpath):
    """The EXEMPT pattern this file is driven by, if any."""
    for pattern, _ in EXEMPT:
        if fnmatch(relpath, pattern):
            return pattern
    return None


def definitions(text):
    names = set()
    for line in text.splitlines():
        match = DEFINITION.match(line)
        if match and match.group(1) != 'main':
            names.add(match.group(1))
    return names


def resolve_module(referrer_dir, module):
    """Where `use <module>` lands, searching outwards from the importing file.

    Two test directories can carry the same module name -- `constants/scoped.qd`
    means `constants/test_constants`, not the `test_constants` sitting at the
    root -- so the nearest one wins and the other is not reached at all.
    """
    directory = referrer_dir
    while True:
        candidate = os.path.join(directory, module)
        if os.path.isdir(candidate):
            return candidate
        parent = os.path.dirname(directory)
        if parent == directory or not parent:
            return None
        directory = parent


def references(referrer, target, text, code, defined):
    """True if `referrer` reaches `target`, by path, module name or symbol."""
    referrer_dir = os.path.dirname(referrer)
    target_dir = os.path.dirname(target)
    module = os.path.basename(target_dir)

    relative = os.path.relpath(target, referrer_dir)
    if not relative.startswith('..'):
        if re.search(rf'\buse\s+"?{re.escape(relative)}"?', text):
            return True

    names_module = bool(re.search(rf'\buse\s+{re.escape(module)}\b', code) or
                        re.search(rf'\b{re.escape(module)}::', code)) and \
        resolve_module(referrer_dir, module) == target_dir
    if names_module and os.path.basename(target) == 'module.qd':
        return True

    # Files of one module share a namespace and are used by symbol alone --
    # `helper.qd` next to `main.qd` needs no `use` at all, and a module's second
    # file is reached as `module::symbol`. Naming the module is not enough on
    # its own: it would vouch for every file in the directory, including one
    # nobody calls into. Only look inside the module, where a name cannot
    # collide with an unrelated helper elsewhere in the tree.
    if referrer_dir == target_dir or names_module:
        for name in defined:
            if re.search(rf'\b{re.escape(name)}\b', code):
                return True
    return False


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join('tests', 'qd')

    tests = []
    for dirpath, _, filenames in os.walk(root):
        for name in sorted(filenames):
            if name.endswith('.qd'):
                tests.append(os.path.join(dirpath, name))
    tests.sort()

    text = {}
    for path in tests:
        with open(path, encoding='utf-8') as handle:
            text[path] = strip_comments(handle.read())
    code = {path: strip_strings(text[path]) for path in tests}
    defined = {path: definitions(code[path]) for path in tests}

    covered = set()
    unpinned = []
    pinned = 0
    matched = {pattern: 0 for pattern, _ in EXEMPT}
    for path in tests:
        pattern = exemption(os.path.relpath(path, root))
        if pattern is not None:
            matched[pattern] += 1
            covered.add(path)
        elif is_pinned(path):
            pinned += 1
            covered.add(path)
        else:
            unpinned.append(path)

    # A helper reached only through another helper is still reached, so keep
    # widening the covered set until it stops growing.
    changed = True
    while changed:
        changed = False
        for path in list(unpinned):
            # A name the referrer defines itself is its own, however much it
            # looks like the helper's: two unrelated compile_errors tests both
            # declaring `shout` is not one importing the other.
            if any(references(referrer, path, text[referrer], code[referrer],
                              defined[path] - defined[referrer])
                   for referrer in covered):
                covered.add(path)
                unpinned.remove(path)
                changed = True

    problems = [f"error: {path}: no .out/.expected/.err/.runtime_err sibling, and no "
                f"test imports it -- it is compiled and run by nobody"
                for path in unpinned]
    problems += [f"error: no test matches exempt pattern '{pattern}' -- "
                 f"stale entry in {__file__}"
                 for pattern, count in matched.items() if count == 0]

    if problems:
        for problem in problems:
            print(problem, file=sys.stderr)
        print(f"\n{len(problems)} problem(s)", file=sys.stderr)
        return 1

    print(f"{len(tests)} tests under {root}: {pinned} pin an expectation, "
          f"{sum(matched.values())} are asserted by another suite, "
          f"{len(tests) - pinned - sum(matched.values())} are helpers imported by one of those")
    return 0


if __name__ == '__main__':
    sys.exit(main())
