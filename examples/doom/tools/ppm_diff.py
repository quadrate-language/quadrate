#!/usr/bin/env python3
"""Per-pixel diff between two binary PPM (P6) images.

Used to quantify how close the Quadrate Doom renderer's output is to
vanilla C Doom's renderer at the same camera position. Pair with the
`-probe` flag on the Quadrate binary, which writes `build/probe_*.ppm`
at a fixed set of (map, camera_x, camera_y, camera_angle) tuples.

Usage
-----
Single pair:
    tools/ppm_diff.py build/probe_01_e1m1_spawn_n.ppm  ref/probe_01_e1m1_spawn_n.ppm

Whole sweep (prints a table, one row per probe filename found in both
dirs, ordered by mean-diff descending):
    tools/ppm_diff.py --sweep build/  ref/

Exit status is the integer-percent-different pixels of the highest-
scoring probe, clamped to [0, 100]. Useful as a CI threshold signal.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    """Return (width, height, raw_rgb_bytes). Accepts P6 only."""
    with path.open("rb") as f:
        data = f.read()
    # Header: "P6\n<w> <h>\n<maxval>\n" (maxval must be 255 for our files)
    if not data.startswith(b"P6"):
        raise ValueError(f"{path}: not a P6 PPM")
    # Walk past the three whitespace-separated header tokens after "P6".
    i = 2
    fields: list[bytes] = []
    while len(fields) < 3 and i < len(data):
        while i < len(data) and data[i] in b" \t\r\n":
            i += 1
        # Skip '#'-style comments that some writers produce.
        if i < len(data) and data[i:i+1] == b"#":
            while i < len(data) and data[i] != 0x0a:
                i += 1
            continue
        start = i
        while i < len(data) and data[i] not in b" \t\r\n":
            i += 1
        fields.append(data[start:i])
    if len(fields) != 3:
        raise ValueError(f"{path}: malformed PPM header")
    w, h, maxv = (int(x) for x in fields)
    if maxv != 255:
        raise ValueError(f"{path}: maxval {maxv} not supported")
    # Skip the single whitespace after maxval, then the rest is raw.
    if i < len(data) and data[i] in b" \t\r\n":
        i += 1
    pixels = data[i:]
    if len(pixels) != w * h * 3:
        raise ValueError(
            f"{path}: expected {w*h*3} pixel bytes, got {len(pixels)}")
    return w, h, pixels


def diff_stats(a: bytes, b: bytes) -> dict[str, float]:
    """Per-pixel abs-diff stats between two equal-length RGB buffers."""
    if len(a) != len(b):
        raise ValueError("buffer length mismatch")
    n_pixels = len(a) // 3
    total_abs = 0
    max_abs = 0
    diff_count = 0   # pixels where any channel differs by > 0
    over5 = 0        # pixels where max channel diff > 5
    over32 = 0       # pixels where max channel diff > 32
    for i in range(0, len(a), 3):
        da = abs(a[i] - b[i])
        dg = abs(a[i + 1] - b[i + 1])
        db = abs(a[i + 2] - b[i + 2])
        m = max(da, dg, db)
        total_abs += da + dg + db
        if m > max_abs:
            max_abs = m
        if m > 0:
            diff_count += 1
        if m > 5:
            over5 += 1
        if m > 32:
            over32 += 1
    return {
        "mean_abs_channel": total_abs / (n_pixels * 3),
        "max_channel": float(max_abs),
        "pct_any_diff": 100.0 * diff_count / n_pixels,
        "pct_over_5": 100.0 * over5 / n_pixels,
        "pct_over_32": 100.0 * over32 / n_pixels,
    }


def compare_one(a_path: Path, b_path: Path) -> dict[str, float]:
    wa, ha, pa = read_ppm(a_path)
    wb, hb, pb = read_ppm(b_path)
    if (wa, ha) != (wb, hb):
        raise ValueError(
            f"size mismatch: {a_path}={wa}x{ha} vs {b_path}={wb}x{hb}")
    return diff_stats(pa, pb)


def run_sweep(a_dir: Path, b_dir: Path) -> int:
    a_files = sorted(a_dir.glob("probe_*.ppm"))
    b_files = {p.name: p for p in b_dir.glob("probe_*.ppm")}
    rows: list[tuple[str, dict[str, float]]] = []
    for ap in a_files:
        bp = b_files.get(ap.name)
        if bp is None:
            print(f"!! missing in {b_dir}: {ap.name}", file=sys.stderr)
            continue
        rows.append((ap.name, compare_one(ap, bp)))
    if not rows:
        print("no matching probes found", file=sys.stderr)
        return 1
    rows.sort(key=lambda r: -r[1]["mean_abs_channel"])
    widest = max(len(r[0]) for r in rows)
    hdr = f"{'probe'.ljust(widest)}   mean   max   any%   >5%    >32%"
    print(hdr)
    print("-" * len(hdr))
    worst_pct = 0.0
    for name, s in rows:
        print(
            f"{name.ljust(widest)}  {s['mean_abs_channel']:5.2f}  "
            f"{int(s['max_channel']):3d}   {s['pct_any_diff']:5.1f}  "
            f"{s['pct_over_5']:5.1f}  {s['pct_over_32']:5.1f}"
        )
        if s["pct_over_5"] > worst_pct:
            worst_pct = s["pct_over_5"]
    return min(int(worst_pct), 100)


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(
        description=(__doc__ or "").splitlines()[0])
    p.add_argument("--sweep", action="store_true",
                   help="compare all probe_*.ppm files found in both dirs")
    p.add_argument("a", help="first PPM (or directory with --sweep)")
    p.add_argument("b", help="second PPM (or directory with --sweep)")
    args = p.parse_args(argv)

    a = Path(args.a)
    b = Path(args.b)
    if args.sweep:
        return run_sweep(a, b)
    stats = compare_one(a, b)
    for k in ("mean_abs_channel", "max_channel",
              "pct_any_diff", "pct_over_5", "pct_over_32"):
        print(f"{k:20s} {stats[k]:.3f}")
    return min(int(stats["pct_over_5"]), 100)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
