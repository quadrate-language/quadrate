#!/usr/bin/env python3
"""Fail if `pub var` does not export, or a bare `var` does.

A module global is reachable from outside the LLVM module only if `pub` reaches
its linkage. It did not: every `var` was emitted `InternalLinkage` and `isPublic()`
was never read, so a `pub var` could not be shared with assembly. In freestanding
mode, where `pub fn` is deliberately given external linkage so assembly can call
it, that made module-level mutable state unusable for the one kind of program that
needs it -- examples/kernel declares its globals in a .S file and reaches them
through an address getter for exactly this reason.

The symbol is `qd_global_<name>`, which is what assembly writes.

Usage: tools/check_global_var_linkage.py [path-to-quadc]
"""

import os
import re
import subprocess
import sys
import tempfile

SOURCE = """pub var exported_slot:i64 = 0
var internal_slot:i64 = 0

fn main() {
\texported_slot internal_slot + drop
}
"""

# `@qd_global_x = global ...` is external; `@qd_global_x = internal global ...` is not.
DEF = re.compile(r'^@(qd_global_\w+)\s*=\s*([a-z ]*?)global\b')


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    quadc = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, 'build', 'debug', 'cmd', 'quadc', 'quadc')
    if not os.path.exists(quadc):
        print(f'quadc not found at {quadc}', file=sys.stderr)
        return 1

    env = dict(os.environ)
    env.setdefault('QUADRATE_ROOT', os.path.join(root, 'dist', 'share', 'quadrate'))
    env.setdefault('QUADRATE_LIBDIR', os.path.join(root, 'dist', 'lib'))

    with tempfile.TemporaryDirectory() as tmp:
        # Its own directory: sibling .qd files in the same one are loaded as a
        # merged module, which would collide with this program's `main`.
        src = os.path.join(tmp, 'linkage.qd')
        with open(src, 'w', encoding='utf-8') as handle:
            handle.write(SOURCE)
        proc = subprocess.run([quadc, '--dump-ir', src, '-o', os.path.join(tmp, 'linkage')],
                              capture_output=True, text=True, env=env)
        if proc.returncode != 0:
            print('compiling the linkage probe failed:\n' + proc.stderr, file=sys.stderr)
            return 1
        ir = proc.stdout + proc.stderr

    linkage = {}
    for line in ir.splitlines():
        match = DEF.match(line.strip())
        if match:
            linkage[match.group(1)] = match.group(2).strip()

    problems = []
    if 'qd_global_exported_slot' not in linkage:
        problems.append("no definition of 'qd_global_exported_slot' in the IR")
    elif linkage['qd_global_exported_slot'] != '':
        problems.append("'pub var exported_slot' is emitted "
                        f"'{linkage['qd_global_exported_slot']} global'; it must be external so "
                        'assembly and other objects can reach it')

    if 'qd_global_internal_slot' not in linkage:
        problems.append("no definition of 'qd_global_internal_slot' in the IR")
    elif linkage['qd_global_internal_slot'] != 'internal':
        problems.append("'var internal_slot' is emitted "
                        f"'{linkage['qd_global_internal_slot']} global'; without 'pub' it must stay internal")

    for problem in problems:
        print(problem, file=sys.stderr)
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
