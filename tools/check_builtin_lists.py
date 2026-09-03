#!/usr/bin/env python3
"""Cross-check every hand-maintained copy of the builtin/keyword surface.

The compiler's tables in lib/qc/include/quadrate/qc/instructions.h are the source
of truth. The same names are also written out by hand in the language reference,
two JSON API files, the Pygments lexer, the playground and the MCP server's help
text. Nothing compared them, so removing a builtin meant editing every list from
memory -- and a miss stayed invisible: `ctx` survived in docs/api/language.json,
which the MCP server serves live, long after the keyword was gone.

Two invariants, both cheap and both violated at some point:

  1. No removed name appears in any list.
  2. Every builtin a list names actually exists.

Documented type-parameterised forms (`cast<T>`) map to their base instruction.

Usage: tools/check_builtin_lists.py [--verbose]
Exit status is non-zero if any list disagrees with the compiler.
"""

import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

INSTRUCTIONS_H = "lib/qc/include/quadrate/qc/instructions.h"
REFERENCE_DEF = "lib/qc/include/quadrate/qc/reference.def"
BUILTINS_JSON = "docs/api/builtins.json"
LANGUAGE_JSON = "docs/api/language.json"
PYGMENTS_LEXER = "docs/pygments-quadrate/quadrate_lexer/__init__.py"

# Files that mention instruction names only in prose or in a syntax-highlighting
# array. Structured parsing buys nothing here, so they get a word-boundary scan
# for removed names -- which is the invariant that actually broke.
PROSE_SOURCES = [
    "tools/playground/templates/index.html",
    "cmd/quadmcp/tools.qd",
    "cmd/quadmcp/resources.qd",
]


def read(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8") as f:
        return f.read()


def strip_line_comments(text):
    return re.sub(r"//[^\n]*", "", text)


def parse_instructions_h():
    """Returns (builtins, removed_instructions, removed_keywords)."""
    src = read(INSTRUCTIONS_H)

    def string_list(start_marker, end_marker):
        block = src[src.index(start_marker):src.index(end_marker)]
        return re.findall(r'"((?:[^"\\]|\\.)*)"', strip_line_comments(block))

    builtins = string_list("BUILTIN_INSTRUCTIONS[] = {", "BUILTIN_INSTRUCTION_COUNT")
    # Each removed entry is {name, effect, replacement}; only the name is a list entry.
    removed_block = src[src.index("REMOVED_INSTRUCTIONS[] = {"):src.index("REMOVED_INSTRUCTION_COUNT")]
    removed = re.findall(r'\{"([^"]+)"', strip_line_comments(removed_block))
    keyword_block = src[src.index("REMOVED_KEYWORDS[] = {"):src.index("REMOVED_KEYWORD_COUNT")]
    removed_kw = re.findall(r'\{"([^"]+)"', strip_line_comments(keyword_block))
    return set(builtins), set(removed), set(removed_kw)


def parse_reference_def():
    src = read(REFERENCE_DEF)
    builtins = re.findall(r"^BUILTIN\(([^,]+),", src, re.M)
    keywords = re.findall(r"^KEYWORD\(([^,]+),", src, re.M)
    return [b.strip() for b in builtins], [k.strip() for k in keywords]


def parse_builtins_json():
    d = json.loads(read(BUILTINS_JSON))
    return [i["name"] for c in d["categories"] for i in c["instructions"]]


def parse_language_json():
    d = json.loads(read(LANGUAGE_JSON))
    return [k["name"] for k in d["keywords"]]


def parse_pygments():
    """Returns (builtins, keywords) from the lexer's `builtins_*` / `keywords` tuples."""
    src = read(PYGMENTS_LEXER)
    builtins, keywords = [], []
    for name, body in re.findall(r"^    (\w+) = \(\n(.*?)^    \)", src, re.M | re.S):
        entries = re.findall(r"'([^']*)'", body)
        # builtins_math holds math:: module functions, not core instructions.
        if name.startswith("builtins") and name != "builtins_math":
            builtins += entries
        elif name == "keywords":
            keywords += entries
    return builtins, keywords


def base_name(name):
    """`cast<T>` documents the `cast` instruction. `<` and `<=` are instructions."""
    m = re.match(r"^(\w+)<", name)
    return m.group(1) if m else name


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument(
        "--extra",
        metavar="PATH",
        nargs="+",
        default=[],
        help="Additional files to word-boundary scan for removed names. Paths are "
             "absolute or relative to the current directory, not to the repo root, "
             "so the editor integrations -- which live in sibling repos and so cannot "
             "be listed in PROSE_SOURCES -- can be checked from their own CI.",
    )
    args = ap.parse_args()

    builtins, removed, removed_kw = parse_instructions_h()
    gone = removed | removed_kw
    problems = []

    ref_builtins, ref_keywords = parse_reference_def()
    pyg_builtins, pyg_keywords = parse_pygments()

    named_builtins = [
        (REFERENCE_DEF, ref_builtins),
        (BUILTINS_JSON, parse_builtins_json()),
        (PYGMENTS_LEXER, pyg_builtins),
    ]
    named_keywords = [
        (REFERENCE_DEF, ref_keywords),
        (LANGUAGE_JSON, parse_language_json()),
        (PYGMENTS_LEXER, pyg_keywords),
    ]

    for where, names in named_builtins + named_keywords:
        for name in names:
            if base_name(name) in gone:
                problems.append(f"{where}: lists '{name}', which was removed")

    for where, names in named_builtins:
        for name in names:
            if base_name(name) not in builtins:
                problems.append(f"{where}: documents '{name}', which is not a builtin")

    # PROSE_SOURCES are scanned for removed *instructions* only. Removed keywords
    # cannot be scanned here: these files embed JavaScript, and the playground's own
    # highlighter is full of `while (i < code.length)`. Instruction names like `tuck`
    # and `dupd` are distinctive enough not to collide.
    prose = [(rel, read(rel), removed) for rel in PROSE_SOURCES]

    # --extra files are syntax definitions -- keyword lists, tree-sitter rules,
    # TextMate patterns -- where every name is a claim about Quadrate, so removed
    # keywords count too: a grammar carrying a `while` or `ctx` rule parses what the
    # compiler rejects.
    for path in args.extra:
        if not os.path.exists(path):
            problems.append(f"{path}: --extra file does not exist")
            continue
        with open(path, encoding="utf-8") as f:
            prose.append((path, f.read(), removed | removed_kw))

    for rel, text, banned in prose:
        for name in sorted(banned):
            for m in re.finditer(r"\b%s\b" % re.escape(name), text):
                line = text.count("\n", 0, m.start()) + 1
                problems.append(f"{rel}:{line}: mentions removed name '{name}'")

    if args.verbose:
        print(f"{len(builtins)} builtins, {len(removed)} removed instructions, "
              f"{len(removed_kw)} removed keywords")
        for where, names in named_builtins + named_keywords:
            print(f"  {where}: {len(names)} names")

    if problems:
        for p in problems:
            print(f"error: {p}", file=sys.stderr)
        print(f"\n{len(problems)} inconsistenc{'y' if len(problems) == 1 else 'ies'} found",
              file=sys.stderr)
        return 1

    print("builtin lists agree with the compiler")
    return 0


if __name__ == "__main__":
    sys.exit(main())
