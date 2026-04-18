#!/usr/bin/env python3
"""Generate examples/doom/qd/sounds.qd from doomgeneric's sounds.h + sounds.c.

Doom's sound content is ~107 music tracks and ~117 sound effects. Each
music entry is just a name string; each sfx has a name + priority +
optional link (points into the sfx table). The enums in sounds.h are
dense 0..N identifiers so we emit them as plain `pub const` values.

Like gen_info.py we allocate the data buffers on first use and expose
accessor functions so callers don't touch byte offsets directly.

Run with:
    python3 tools/gen_sounds.py \
        --src $HOME/dev/github/ozkl/doomgeneric/doomgeneric \
        --out qd/sounds.qd
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def extract_enum(source: str, name: str) -> list[str]:
    pattern = re.compile(
        r"typedef\s+enum\s*(?:\{\s*)?"
        r"((?:[^{}]|\{[^{}]*\})*?)"
        r"\}\s*" + re.escape(name) + r"\s*;",
        re.DOTALL,
    )
    m = pattern.search(source)
    if not m:
        raise SystemExit(f"couldn't find enum '{name}'")
    out = []
    for line in m.group(1).splitlines():
        line = line.split("//", 1)[0].strip().rstrip(",")
        if not line:
            continue
        ident = line.split("=", 1)[0].strip()
        if ident:
            out.append(ident)
    return out


def extract_music(source: str) -> list[str | None]:
    m = re.search(
        r"musicinfo_t\s+S_music\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;",
        source,
        re.DOTALL,
    )
    if not m:
        raise SystemExit("couldn't find S_music[] in sounds.c")
    body = m.group(1)
    entries: list[str | None] = []
    for m2 in re.finditer(r"MUSIC\(\s*(NULL|\"[^\"]*\")\s*\)", body):
        v = m2.group(1)
        entries.append(None if v == "NULL" else v[1:-1])
    return entries


def extract_sfx(source: str) -> list[tuple[str, int, int | None, int, int]]:
    """Return (name, priority, link_idx_or_None, pitch, volume) per entry."""
    m = re.search(
        r"sfxinfo_t\s+S_sfx\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;",
        source,
        re.DOTALL,
    )
    if not m:
        raise SystemExit("couldn't find S_sfx[] in sounds.c")
    body = m.group(1)
    entries: list[tuple[str, int, int | None, int, int]] = []
    for m2 in re.finditer(
        r"SOUND(?:_LINK)?\(\s*\"([^\"]*)\"\s*,\s*(-?\d+)"
        r"(?:\s*,\s*(\w+)\s*,\s*(-?\d+)\s*,\s*(-?\d+))?\s*\)",
        body,
    ):
        name = m2.group(1)
        priority = int(m2.group(2))
        link_name = m2.group(3)
        pitch = int(m2.group(4)) if m2.group(4) else -1
        volume = int(m2.group(5)) if m2.group(5) else -1
        # link_name is an sfx_* enum ID. We resolve it at emit time with the
        # sfx_enum list once we've built it.
        entries.append((name, priority, link_name, pitch, volume))
    return entries


SFX_STRIDE = 32                 # name:9 bytes + 3 padding + priority:i32 + link:i32 + pitch:i32 + volume:i32 + 4 pad


def emit(out_path: Path, music_enums, sfx_enums, music_entries, sfx_entries):
    lines: list[str] = []
    w = lines.append

    w("""/// Sound-effect and music tables.
///
/// Auto-generated from sounds.c + sounds.h by tools/gen_sounds.py. Do not
/// edit by hand — regenerate.
///
/// Layout:
///   * music names stored as a single NUL-delimited blob `mus_names_blob`,
///     indexed by `mus_name_offsets[i]`. Access with `music_name(i)`.
///   * sfx entries packed as:
///       0..9   char name[9]
///       9..12  (padding)
///       12..16 priority:i32
///       16..20 link:i32 (index into S_sfx, 0 for none)
///       20..24 pitch:i32
///       24..28 volume:i32
///       28..32 (padding)
///     Accessors: `sfx_name(i)`, `sfx_priority(i)`, `sfx_link(i)` …

use mem

/// Enum constants — music track IDs.""")
    for i, n in enumerate(music_enums):
        w(f"pub const {n} = {i}")
    w(f"pub const NUMMUSIC = {len(music_enums) - 1}")
    w("")
    w("/// Enum constants — sound-effect IDs.")
    for i, n in enumerate(sfx_enums):
        w(f"pub const {n} = {i}")
    w(f"pub const NUMSFX = {len(sfx_enums) - 1}")
    w("")
    w(f"pub const SFX_STRIDE = {SFX_STRIDE}")
    w("")

    w("pub var sfx_table:ptr = 0")
    w("pub var mus_names_blob:ptr = 0     // NUL-delimited music names")
    w("pub var mus_name_offsets:ptr = 0   // i32 per music index")
    w("var sounds_ready:i64 = 0")
    w("")

    # Build the music-names blob ahead of time so we know the size.
    name_blob = bytearray()
    name_offsets: list[int] = []
    for entry in music_entries:
        name_offsets.append(len(name_blob))
        if entry is not None:
            name_blob.extend(entry.encode("ascii"))
        name_blob.append(0)

    w(f"const MUS_BLOB_BYTES = {len(name_blob)}")
    w(f"const NUM_MUS_ENTRIES = {len(music_entries)}")
    w("")

    # Resolve sfx link names to indices.
    sfx_index = {n: i for i, n in enumerate(sfx_enums)}

    w("""fn put_byte(p:ptr off:i64 v:i64 -- ) { v p off mem::set_u8 }
fn put_i32(p:ptr off:i64 v:i64 -- ) { v p off mem::set_i32 }

pub fn ensure_sounds( -- ) {
	sounds_ready 0 neq if { return }
""")

    # sfx table.
    w(f"\tNUMSFX 1 add SFX_STRIDE mul mem::alloc! -> sfx_table")
    w(f"\tsfx_table NUMSFX 1 add SFX_STRIDE mul mem::zero")
    for i, (name, priority, link_name, pitch, volume) in enumerate(sfx_entries):
        base = i * SFX_STRIDE
        # mem::set_u8  takes (value address offset). Write name bytes.
        for j, ch in enumerate(name[:8]):
            w(f"\t{ord(ch)} sfx_table {base + j} mem::set_u8")
        w(f"\t{priority} sfx_table {base + 12} mem::set_i32")
        link_idx = sfx_index.get(link_name or "", 0) if link_name else 0
        w(f"\t{link_idx} sfx_table {base + 16} mem::set_i32")
        w(f"\t{pitch} sfx_table {base + 20} mem::set_i32")
        w(f"\t{volume} sfx_table {base + 24} mem::set_i32")

    # music names blob.
    w("")
    w(f"\tMUS_BLOB_BYTES mem::alloc! -> mus_names_blob")
    for i, b in enumerate(name_blob):
        if b != 0:
            w(f"\t{b} mus_names_blob {i} mem::set_u8")

    w(f"\tNUM_MUS_ENTRIES 4 mul mem::alloc! -> mus_name_offsets")
    for i, off in enumerate(name_offsets):
        w(f"\t{off} mus_name_offsets {i * 4} mem::set_i32")

    w("\t1 -> sounds_ready")
    w("}")
    w("")

    # Accessors.
    w(f"""pub fn sfx_name(i:i64 -- p:ptr len:i64) {{
	ensure_sounds
	sfx_table i SFX_STRIDE mul mem::ptr_add -> base
	// name is NUL-terminated up to 8 chars; scan for the end.
	0 -> n
	loop {{
		n 8 gte if {{ break }}
		base n mem::get_u8 0 eq if {{ break }}
		n 1 add -> n
	}}
	base n
}}

pub fn sfx_priority(i:i64 -- v:i64) {{
	ensure_sounds
	sfx_table i SFX_STRIDE mul mem::ptr_add -> p
	p 12 mem::get_i32
}}

pub fn sfx_link(i:i64 -- v:i64) {{
	ensure_sounds
	sfx_table i SFX_STRIDE mul mem::ptr_add -> p
	p 16 mem::get_i32
}}

pub fn sfx_pitch(i:i64 -- v:i64) {{
	ensure_sounds
	sfx_table i SFX_STRIDE mul mem::ptr_add -> p
	p 20 mem::get_i32
}}

pub fn sfx_volume(i:i64 -- v:i64) {{
	ensure_sounds
	sfx_table i SFX_STRIDE mul mem::ptr_add -> p
	p 24 mem::get_i32
}}

/// Music track name as a ptr+length. Empty length for mus_None.
pub fn music_name(i:i64 -- p:ptr len:i64) {{
	ensure_sounds
	mus_name_offsets i 4 mul mem::get_i32 -> off
	mus_names_blob off mem::ptr_add -> base
	0 -> n
	loop {{
		base n mem::get_u8 0 eq if {{ break }}
		n 1 add -> n
	}}
	base n
}}
""")

    out_path.write_text("\n".join(lines) + "\n")
    print(f"wrote {out_path} — {len(music_enums) - 1} music, {len(sfx_enums) - 1} sfx")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    src = Path(args.src)
    header = (src / "sounds.h").read_text()
    body = (src / "sounds.c").read_text()

    music_enums = extract_enum(header, "musicenum_t")
    sfx_enums = extract_enum(header, "sfxenum_t")
    music_entries = extract_music(body)
    sfx_entries = extract_sfx(body)

    emit(Path(args.out), music_enums, sfx_enums, music_entries, sfx_entries)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
