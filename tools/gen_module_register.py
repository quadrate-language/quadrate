#!/usr/bin/env python3
"""Emit a module's native registration table from its .qd import block.

A stdlib module declares its C-backed words once, in the `import "lib<mod>.a"`
block of its .qd source, and the compiler reads them from there. The
interpreter cannot: it resolves a word by asking the context for a registered
native, so something has to call qd_native_register() for each one. Generating
that call from the same declaration keeps the two views of a module from
drifting -- a word added to the block is registered without a second edit.

Usage: gen_module_register.py --module <name> <input.qd> <output.c>
"""

import argparse
import re
import sys

# `pub fn name(effect)` or `fn name(effect)`, with an optional trailing `!`
# marking a fallible word. Only the parenthesised stack effect is kept; the
# runtime's arity check reads it back.
DECL = re.compile(r"^\s*(?:pub\s+)?(?:inline\s+)?fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*(\([^)]*\))\s*!?\s*$")

# Anything that opens a declaration. A line that starts one but does not match
# DECL -- a stack effect wrapped across lines, say -- is a hard error rather
# than a skip: skipping it would build fine and leave the word unregistered at
# runtime, which is the one failure this generator exists to prevent.
DECL_START = re.compile(r"^\s*(?:pub\s+)?(?:inline\s+)?fn\b")


def parse_block(source, module):
    """Words declared in the import block backing lib<module>.a.

    Other import blocks in the same file (a module may import lib/rt's words)
    belong to another archive and are skipped.
    """
    opening = re.compile(r'^\s*import\s+"lib' + re.escape(module) + r'\.a"\s+as\s+"([^"]+)"\s*\{')

    alias = None
    words = []
    for line in source.splitlines():
        if alias is None:
            match = opening.match(line)
            if match:
                alias = match.group(1)
            continue

        if line.startswith("}"):
            break

        match = DECL.match(line)
        if match:
            words.append((match.group(1), match.group(2)))
        elif DECL_START.match(line):
            raise ValueError(f"cannot parse declaration: {line.strip()}")

    return alias, words


def render(module, alias, words, source_path):
    """The registration table, plus one thunk per word.

    A stdlib native is `int f(qd_context*)`, while qd_native_callback is
    `int (*)(qd_context*, void*)`. A thunk per word adapts the two without
    casting a function pointer, which C does not guarantee round-trips
    through void*. They are static and tiny; --gc-sections drops the unused
    ones along with the natives behind them.
    """
    lines = [
        f"// Generated from {source_path} by tools/gen_module_register.py.",
        "// Do not edit: add the word to the .qd import block instead.",
        "",
        f"#include <quadrate/{module}/{module}.h>",
        "#include <quadrate/rt/runtime.h>",
        "",
        "// Prototypes are emitted rather than taken from the module header: the",
        "// import block is the authority on what the archive backs, and several",
        "// modules implement words their public header does not declare.",
    ]

    for name, _ in words:
        lines.append(f"int usr_{alias}_{name}(qd_context* ctx);")

    lines.append("")

    for name, _ in words:
        lines += [
            f"static int qd_thunk_{alias}_{name}(qd_context* ctx, void* userdata) {{",
            "\t(void)userdata;",
            f"\treturn usr_{alias}_{name}(ctx);",
            "}",
        ]

    lines += [
        "",
        f"bool qd_{module}_register(qd_context* ctx) {{",
        "\tif (ctx == NULL) {",
        "\t\treturn false;",
        "\t}",
        "",
    ]

    for name, effect in words:
        lines += [
            f'\tif (!qd_native_register(ctx, "{alias}::{name}", "{effect}",',
            f"\t\t\tqd_thunk_{alias}_{name}, NULL)) {{",
            "\t\treturn false;",
            "\t}",
        ]

    lines += ["", "\treturn true;", "}", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", required=True, help="module name, e.g. math")
    parser.add_argument("source", help="the module's .qd source")
    parser.add_argument("output", help="C file to write")
    args = parser.parse_args()

    with open(args.source, encoding="utf-8") as handle:
        source = handle.read()

    try:
        alias, words = parse_block(source, args.module)
    except ValueError as err:
        print(f"{args.source}: {err}", file=sys.stderr)
        return 1
    if alias is None:
        print(f"{args.source}: no import block for lib{args.module}.a", file=sys.stderr)
        return 1
    if not words:
        print(f"{args.source}: import block for lib{args.module}.a declares no words", file=sys.stderr)
        return 1

    with open(args.output, "w", encoding="utf-8") as handle:
        handle.write(render(args.module, alias, words, args.source.split("/")[-1]))

    return 0


if __name__ == "__main__":
    sys.exit(main())
