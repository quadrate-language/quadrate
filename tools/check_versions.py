#!/usr/bin/env python3
"""Verify that every hand-maintained version string matches the toolchain version.

The C++ tools take their version from the git tag at build time. quadmcp is a
Quadrate program and carries its own constant, which drifted to 0.2.0 while the
toolchain reported 0.5.0. This keeps them in step.
"""
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def toolchain_version() -> str:
    try:
        out = subprocess.run(
            ["git", "describe", "--tags", "--abbrev=0"],
            cwd=ROOT, capture_output=True, text=True, check=True,
        ).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""
    return out.lstrip("v")


def main() -> int:
    expected = toolchain_version()
    if not expected:
        print("check_versions: no git tag available, skipping")
        return 0

    problems = []

    core = (ROOT / "cmd/quadmcp/core.qd").read_text()
    m = re.search(r'const SERVER_VERSION = "([^"]+)"', core)
    if not m:
        problems.append("cmd/quadmcp/core.qd: SERVER_VERSION not found")
    elif m.group(1) != expected:
        problems.append(f"cmd/quadmcp/core.qd: SERVER_VERSION is {m.group(1)}, toolchain is {expected}")

    manifest = json.loads((ROOT / "cmd/quadmcp/qd.json").read_text())
    if manifest.get("version") != expected:
        problems.append(f"cmd/quadmcp/qd.json: version is {manifest.get('version')}, toolchain is {expected}")

    for p in problems:
        print(p)
    if problems:
        return 1
    print(f"check_versions: all versions match {expected}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
