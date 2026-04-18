#!/usr/bin/env python3
"""Generate examples/doom/qd/info.qd from doomgeneric's info.h + info.c.

Parses the three tables Doom ships as compile-time C initializers:

  * sprnames[]               — ~138 4-char sprite names
  * states[NUMSTATES]        — ~967 animation frames with action-function pointers
  * mobjinfo[NUMMOBJTYPES]   — ~137 monster/item prototypes with 23 i32 fields

Writes them out as Quadrate:

  * Constants for every enum value (SPR_*, S_*, MT_*)
  * A `ensure_info` function that lazily populates packed buffers at first use
  * Bare stub definitions for every A_* action routine — real bodies will land
    during the p_enemy / p_pspr port; the stubs just keep the state table from
    dangling.

The goal is a pure-Quadrate equivalent of info.c: no C data remains in the
final build, and everything compiles off the stock info.h + info.c that ship
with doomgeneric. Re-run whenever the upstream data changes:

    python3 tools/gen_info.py \
        --src "$HOME/dev/github/ozkl/doomgeneric/doomgeneric" \
        --out qd/info.qd
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def extract_enum(source: str, name: str) -> list[str]:
    """Return the ordered list of identifiers in a `typedef enum { … } name;`."""
    # Match `typedef enum` with an optional `{`, then capture up to the closing brace.
    pattern = re.compile(
        r"typedef\s+enum\s*(?:\{\s*)?"
        r"((?:[^{}]|\{[^{}]*\})*?)"
        r"\}\s*" + re.escape(name) + r"\s*;",
        re.DOTALL,
    )
    m = pattern.search(source)
    if not m:
        raise SystemExit(f"couldn't find enum '{name}' in info.h")
    body = m.group(1)
    names: list[str] = []
    for line in body.splitlines():
        line = line.split("//", 1)[0].strip().rstrip(",")
        if not line or line.startswith("/*"):
            continue
        # An entry may have an explicit value: "NAME = 3". Doom's info enums
        # are densely packed from 0 so we only need the identifier.
        ident = line.split("=", 1)[0].strip()
        if ident:
            names.append(ident)
    return names


def extract_sprnames(source: str) -> list[str]:
    """Pull the four-letter sprite strings out of `char *sprnames[] = { … };`."""
    m = re.search(r"char\s*\*\s*sprnames\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;", source, re.DOTALL)
    if not m:
        raise SystemExit("couldn't find sprnames[] in info.c")
    names = re.findall(r'"([^"]+)"', m.group(1))
    return names


def extract_states(source: str) -> list[tuple[str, str, str, str, str, str, str]]:
    """Each state_t is `{SPR_X, frame, tics, {A_Whatever}, S_NEXT, misc1, misc2}`."""
    m = re.search(
        r"state_t\s+states\s*\[\s*NUMSTATES\s*\]\s*=\s*\{(.*?)\}\s*;",
        source,
        re.DOTALL,
    )
    if not m:
        raise SystemExit("couldn't find states[] in info.c")
    body = m.group(1)

    states: list[tuple[str, str, str, str, str, str, str]] = []
    # Match `{SPR,frame,tics,{ACTION},NEXT,misc1,misc2}` with whitespace tolerance.
    entry_re = re.compile(
        r"\{\s*([A-Z_][A-Z0-9_]*)\s*,\s*"       # sprite
        r"(-?\d+)\s*,\s*"                        # frame
        r"(-?\d+)\s*,\s*"                        # tics
        r"\{\s*([A-Za-z_0-9]+)\s*\}\s*,\s*"      # action (word or NULL)
        r"([A-Z_][A-Z0-9_]*)\s*,\s*"             # nextstate
        r"(-?\d+)\s*,\s*"                        # misc1
        r"(-?\d+)\s*\}",                         # misc2
        re.DOTALL,
    )
    for m2 in entry_re.finditer(body):
        states.append(m2.groups())
    return states


def extract_mf_flags(p_mobj_source: str) -> dict[str, int]:
    """Pull the MF_* enum values out of p_mobj.h. Values include hex forms."""
    flags: dict[str, int] = {}
    # Match lines like `MF_NAME   = 0x400,`  or `MF_NAME = 123,`.
    for m in re.finditer(r"\b(MF[0-9A-Z_]*)\s*=\s*(0x[0-9a-fA-F]+|\d+)", p_mobj_source):
        flags[m.group(1)] = int(m.group(2), 0)
    return flags


def extract_mobjinfo(source: str) -> list[list[str]]:
    """mobjinfo_t has 23 int fields. Return them in declared order as strings."""
    m = re.search(
        r"mobjinfo_t\s+mobjinfo\s*\[\s*NUMMOBJTYPES\s*\]\s*=\s*\{(.*?)\}\s*;",
        source,
        re.DOTALL,
    )
    if not m:
        raise SystemExit("couldn't find mobjinfo[] in info.c")
    body = m.group(1)

    # Strip line comments — the field initializers use `// painstate` etc.
    body = re.sub(r"//[^\n]*", "", body)
    # Each mobjinfo entry is brace-delimited. Scan at depth 1.
    entries: list[str] = []
    depth = 0
    current: list[str] = []
    for ch in body:
        if ch == "{":
            if depth == 0:
                current = []
            else:
                current.append(ch)
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                entries.append("".join(current))
            else:
                current.append(ch)
        elif depth >= 1:
            current.append(ch)

    out: list[list[str]] = []
    for entry in entries:
        fields = [f.strip() for f in entry.split(",")]
        # Trailing comma / whitespace creates empty fields — filter them.
        fields = [f for f in fields if f != ""]
        if len(fields) != 23:
            raise SystemExit(f"mobjinfo entry doesn't have 23 fields: {fields!r}")
        out.append(fields)
    return out


def collect_actions(states: list[tuple[str, ...]], source: str) -> list[str]:
    """Every A_* name either referenced in the state table or forward-declared."""
    names: set[str] = set()
    for s in states:
        action = s[3]
        if action != "NULL":
            names.add(action)
    # Doomgeneric forward-declares every A_* before the state table. Pick them up
    # too so we don't miss any not yet referenced.
    for m in re.finditer(r"\bvoid\s+(A_[A-Za-z]\w*)\s*\(", source):
        names.add(m.group(1))
    return sorted(names)


# --------------------------------------------------------------------------- #
# Emitter                                                                     #
# --------------------------------------------------------------------------- #

STATE_STRIDE = 32        # see state_t layout comment in info.qd
MOBJINFO_STRIDE = 92     # 23 × 4 bytes


def eval_mobj_field(expr: str, state_enums: list[str], mobj_enums: list[str], mf_flags: dict[str, int]) -> int:
    """Best-effort evaluator for mobjinfo field initializers.

    The C source uses integer literals, FRACUNIT multiplies, enum references
    (S_*, MT_*, sfx_*), and bitwise-OR of MF_* flag constants. We resolve
    what we can; symbols we don't know (sfx_*, unmapped enums) become 0, so
    state/flag fields that Doom actually reads behave correctly while we're
    still stubbing the sound system.
    """
    state_idx = {name: i for i, name in enumerate(state_enums)}
    mobj_idx = {name: i for i, name in enumerate(mobj_enums)}

    # Known numeric constants.
    FRACUNIT = 1 << 16

    # Build a sandbox namespace.
    env: dict[str, int] = {"FRACUNIT": FRACUNIT}
    env.update(mf_flags)
    env.update(state_idx)
    env.update(mobj_idx)

    # Replace unknown identifiers with 0. `sfx_shotgn` and the like aren't
    # in our namespace; walking the tokens is safer than regexes since the
    # expression may contain bitwise-OR with parentheses.
    tokens = re.split(r"(\W+)", expr)
    safe_expr = []
    for t in tokens:
        if t.strip() == "":
            safe_expr.append(t)
            continue
        if re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", t):
            if t in env:
                safe_expr.append(str(env[t]))
            else:
                safe_expr.append("0")
        else:
            safe_expr.append(t)

    try:
        return int(eval("".join(safe_expr), {"__builtins__": {}}, {}))
    except Exception:
        return 0


def emit(
    out_path: Path,
    sprite_enums: list[str],
    state_enums: list[str],
    mobj_enums: list[str],
    sprnames: list[str],
    states: list[tuple[str, str, str, str, str, str, str]],
    mobjinfos: list[list[str]],
    action_names: list[str],
    mf_flags: dict[str, int],
) -> None:
    state_enum_index = {name: i for i, name in enumerate(state_enums)}
    sprite_enum_index = {name: i for i, name in enumerate(sprite_enums)}

    # Actions that are *referenced* from states get real slots so states can
    # store their function pointer. Represent as i64 indices (0 = NULL, 1+ =
    # action table index). The runtime will later look up a fn(ptr --) in a
    # parallel table when it actually needs to call them.
    action_index = {"NULL": 0}
    for idx, name in enumerate(action_names, start=1):
        action_index[name] = idx

    lines: list[str] = []
    w = lines.append

    w(f"""/// Frame / state / mobj tables — auto-generated from info.c by
/// tools/gen_info.py. Do not edit by hand; regenerate instead.
///
/// Data layout:
///   * states: packed array of {STATE_STRIDE}-byte entries holding
///       sprite:i32 frame:i32 tics:i32 action_id:i32 nextstate:i32 misc1:i32 misc2:i32
///     (one slot of padding lets us extend later).
///   * mobjinfo: packed array of {MOBJINFO_STRIDE}-byte entries with 23 i32 fields.
///   * sprnames_ptr: pointer to a 5-byte-per-entry zero-padded buffer.
///
/// action_id maps into `action_table` (filled by the p_enemy / p_pspr port
/// with real fn(ptr --) pointers). Until those arrive, the dispatcher treats
/// every non-zero id as a no-op.

use mem

/// Enum constants — sprite indices (SPR_*).""")
    for i, name in enumerate(sprite_enums):
        w(f"pub const {name} = {i}")
    w("")
    w("/// Enum constants — animation states (S_*).")
    for i, name in enumerate(state_enums):
        w(f"pub const {name} = {i}")
    w("")
    w("/// Enum constants — mobj types (MT_*).")
    for i, name in enumerate(mobj_enums):
        w(f"pub const {name} = {i}")
    w("")
    w(f"pub const NUMSPRITES = {len(sprite_enums) - 1}")
    w(f"pub const NUMSTATES = {len(state_enums) - 1}")
    w(f"pub const NUMMOBJTYPES = {len(mobj_enums) - 1}")
    w(f"pub const STATE_STRIDE = {STATE_STRIDE}")
    w(f"pub const MOBJINFO_STRIDE = {MOBJINFO_STRIDE}")
    w("")
    w("/// Total number of A_* action routines referenced anywhere in info.c.")
    w(f"pub const NUMACTIONS = {len(action_names)}")
    w("")

    # Action id constants so state table builders can use names.
    w("/// Action-function ids. 0 = no action. Values correspond to slots in")
    w("/// the action_table once it's populated by the p_enemy port.")
    w("pub const AC_NULL = 0")
    for name in action_names:
        w(f"pub const AC_{name[2:]} = {action_index[name]}")
    w("")

    w("/// Packed data buffers, allocated on first use.")
    w("pub var states:ptr = 0")
    w("pub var mobjinfo:ptr = 0")
    w("pub var sprnames_ptr:ptr = 0")
    w("var info_ready:i64 = 0")
    w("")

    # Helpers.
    w(f"""/// Write one state_t at index `i`.
fn put_state(i:i64 sprite:i64 frame:i64 tics:i64 action_id:i64 next:i64 m1:i64 m2:i64 -- ) {{
	states i {STATE_STRIDE} mul mem::ptr_add -> p
	sprite    p 0  mem::set_i32
	frame     p 4  mem::set_i32
	tics      p 8  mem::set_i32
	action_id p 12 mem::set_i32
	next      p 16 mem::set_i32
	m1        p 20 mem::set_i32
	m2        p 24 mem::set_i32
}}

/// Write one mobjinfo_t at index `i`. 23 i32 fields.
fn put_mobj(i:i64 f0:i64 f1:i64 f2:i64 f3:i64 f4:i64 f5:i64 f6:i64 f7:i64 f8:i64 f9:i64 f10:i64 f11:i64 f12:i64 f13:i64 f14:i64 f15:i64 f16:i64 f17:i64 f18:i64 f19:i64 f20:i64 f21:i64 f22:i64 -- ) {{
	mobjinfo i {MOBJINFO_STRIDE} mul mem::ptr_add -> p
	f0  p 0   mem::set_i32
	f1  p 4   mem::set_i32
	f2  p 8   mem::set_i32
	f3  p 12  mem::set_i32
	f4  p 16  mem::set_i32
	f5  p 20  mem::set_i32
	f6  p 24  mem::set_i32
	f7  p 28  mem::set_i32
	f8  p 32  mem::set_i32
	f9  p 36  mem::set_i32
	f10 p 40  mem::set_i32
	f11 p 44  mem::set_i32
	f12 p 48  mem::set_i32
	f13 p 52  mem::set_i32
	f14 p 56  mem::set_i32
	f15 p 60  mem::set_i32
	f16 p 64  mem::set_i32
	f17 p 68  mem::set_i32
	f18 p 72  mem::set_i32
	f19 p 76  mem::set_i32
	f20 p 80  mem::set_i32
	f21 p 84  mem::set_i32
	f22 p 88  mem::set_i32
}}

/// Write one 4-char sprite name at slot `i` (name is zero-padded to 5 bytes).
fn put_sprite(i:i64 c0:i64 c1:i64 c2:i64 c3:i64 -- ) {{
	sprnames_ptr i 5 mul mem::ptr_add -> p
	c0 p 0 mem::set_u8
	c1 p 1 mem::set_u8
	c2 p 2 mem::set_u8
	c3 p 3 mem::set_u8
	0  p 4 mem::set_u8
}}

pub fn ensure_info( -- ) {{
	info_ready 0 neq if {{ return }}

	{len(state_enums) - 1} {STATE_STRIDE} mul mem::alloc! -> states
	states {len(state_enums) - 1} {STATE_STRIDE} mul mem::zero

	{len(mobj_enums) - 1} {MOBJINFO_STRIDE} mul mem::alloc! -> mobjinfo
	mobjinfo {len(mobj_enums) - 1} {MOBJINFO_STRIDE} mul mem::zero

	{len(sprite_enums) - 1} 5 mul mem::alloc! -> sprnames_ptr
	sprnames_ptr {len(sprite_enums) - 1} 5 mul mem::zero

	populate_sprnames
	populate_states
	populate_mobjinfo

	1 -> info_ready
}}
""")

    # Emit the table populators.
    w("fn populate_sprnames( -- ) {")
    for i, name in enumerate(sprnames):
        padded = (name + "\0\0\0\0")[:4]
        bytes_ = " ".join(str(ord(c)) for c in padded)
        w(f"\t{i} {bytes_} put_sprite")
    w("}")
    w("")

    w("fn populate_states( -- ) {")
    for i, (sprite, frame, tics, action, next_, m1, m2) in enumerate(states):
        s = sprite_enum_index.get(sprite, 0)
        n = state_enum_index.get(next_, 0)
        a = action_index.get(action, 0)
        w(f"\t{i} {s} {frame} {tics} {a} {n} {m1} {m2} put_state")
    w("}")
    w("")

    w("fn populate_mobjinfo( -- ) {")
    for i, fields in enumerate(mobjinfos):
        resolved = [str(eval_mobj_field(f.strip(), state_enums, mobj_enums, mf_flags))
                   for f in fields]
        w(f"\t{i} " + " ".join(resolved) + " put_mobj")
    w("}")
    w("")

    # Action stubs — empty fn bodies that take a thinker ptr argument so the
    # state-machine caller can invoke them uniformly even before the AI port.
    w("// Action-function stubs. Bodies land in the Phase 4 p_enemy/p_pspr port.")
    for name in action_names:
        w(f"pub fn {name}(mo:ptr -- ) {{ mo drop }}")
    w("")

    # Accessors. Stack discipline is explicit — compute the element address
    # with mem::ptr_add first, then read the field with its intra-element
    # offset via mem::get_i32(addr, offset).
    w(f"""pub fn state_sprite(i:i64 -- v:i64) {{
	ensure_info
	states i STATE_STRIDE mul mem::ptr_add -> p
	p 0 mem::get_i32
}}

pub fn state_frame(i:i64 -- v:i64) {{
	ensure_info
	states i STATE_STRIDE mul mem::ptr_add -> p
	p 4 mem::get_i32
}}

pub fn state_tics(i:i64 -- v:i64) {{
	ensure_info
	states i STATE_STRIDE mul mem::ptr_add -> p
	p 8 mem::get_i32
}}

pub fn state_action(i:i64 -- v:i64) {{
	ensure_info
	states i STATE_STRIDE mul mem::ptr_add -> p
	p 12 mem::get_i32
}}

pub fn state_nextstate(i:i64 -- v:i64) {{
	ensure_info
	states i STATE_STRIDE mul mem::ptr_add -> p
	p 16 mem::get_i32
}}

pub fn state_misc1(i:i64 -- v:i64) {{
	ensure_info
	states i STATE_STRIDE mul mem::ptr_add -> p
	p 20 mem::get_i32
}}

pub fn state_misc2(i:i64 -- v:i64) {{
	ensure_info
	states i STATE_STRIDE mul mem::ptr_add -> p
	p 24 mem::get_i32
}}

/// mobjinfo accessor — `f` is the field index 0..22 matching the C struct
/// order. Prefer named wrappers below in new code.
pub fn mobj_field(i:i64 f:i64 -- v:i64) {{
	ensure_info
	mobjinfo i MOBJINFO_STRIDE mul mem::ptr_add -> p
	p f 4 mul mem::get_i32
}}

pub fn mobj_doomednum(i:i64 -- v:i64)     {{ i 0  mobj_field }}
pub fn mobj_spawnstate(i:i64 -- v:i64)    {{ i 1  mobj_field }}
pub fn mobj_spawnhealth(i:i64 -- v:i64)   {{ i 2  mobj_field }}
pub fn mobj_seestate(i:i64 -- v:i64)      {{ i 3  mobj_field }}
pub fn mobj_seesound(i:i64 -- v:i64)      {{ i 4  mobj_field }}
pub fn mobj_reactiontime(i:i64 -- v:i64)  {{ i 5  mobj_field }}
pub fn mobj_attacksound(i:i64 -- v:i64)   {{ i 6  mobj_field }}
pub fn mobj_painstate(i:i64 -- v:i64)     {{ i 7  mobj_field }}
pub fn mobj_painchance(i:i64 -- v:i64)    {{ i 8  mobj_field }}
pub fn mobj_painsound(i:i64 -- v:i64)     {{ i 9  mobj_field }}
pub fn mobj_meleestate(i:i64 -- v:i64)    {{ i 10 mobj_field }}
pub fn mobj_missilestate(i:i64 -- v:i64)  {{ i 11 mobj_field }}
pub fn mobj_deathstate(i:i64 -- v:i64)    {{ i 12 mobj_field }}
pub fn mobj_xdeathstate(i:i64 -- v:i64)   {{ i 13 mobj_field }}
pub fn mobj_deathsound(i:i64 -- v:i64)    {{ i 14 mobj_field }}
pub fn mobj_speed(i:i64 -- v:i64)         {{ i 15 mobj_field }}
pub fn mobj_radius(i:i64 -- v:i64)        {{ i 16 mobj_field }}
pub fn mobj_height(i:i64 -- v:i64)        {{ i 17 mobj_field }}
pub fn mobj_mass(i:i64 -- v:i64)          {{ i 18 mobj_field }}
pub fn mobj_damage(i:i64 -- v:i64)        {{ i 19 mobj_field }}
pub fn mobj_activesound(i:i64 -- v:i64)   {{ i 20 mobj_field }}
pub fn mobj_flags(i:i64 -- v:i64)         {{ i 21 mobj_field }}
pub fn mobj_raisestate(i:i64 -- v:i64)    {{ i 22 mobj_field }}
""")

    out_path.write_text("\n".join(lines) + "\n")
    print(f"wrote {out_path} — {len(state_enums) - 1} states, {len(mobj_enums) - 1} mobjtypes, {len(action_names)} actions")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="path to doomgeneric source directory")
    ap.add_argument("--out", required=True, help="output .qd path")
    args = ap.parse_args()

    src_dir = Path(args.src)
    header = (src_dir / "info.h").read_text()
    body = (src_dir / "info.c").read_text()
    p_mobj_header = (src_dir / "p_mobj.h").read_text()

    sprite_enums = extract_enum(header, "spritenum_t")
    state_enums = extract_enum(header, "statenum_t")
    mobj_enums = extract_enum(header, "mobjtype_t")
    sprnames = extract_sprnames(body)
    states = extract_states(body)
    mobjinfos = extract_mobjinfo(body)
    actions = collect_actions(states, body)
    mf_flags = extract_mf_flags(p_mobj_header)

    emit(
        Path(args.out),
        sprite_enums,
        state_enums,
        mobj_enums,
        sprnames,
        states,
        mobjinfos,
        actions,
        mf_flags,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
