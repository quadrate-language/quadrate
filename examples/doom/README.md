# doom — Doom ported to Quadrate

An incremental port of [doomgeneric](https://github.com/ozkl/doomgeneric) (a
cleaned-up Doom source fork with a 6-function platform API) to Quadrate.

**Status: Phase 0** — baseline all-C SDL2 build. No Quadrate code yet.

## Why

Doom is the largest, most demanding real-world codebase we've thrown at
Quadrate. Every gap in the type system, stdlib, and tooling shows up. Each
blocker found becomes a motivated language-development task — see
`../../TODO.md` and the plan file.

The endgame (Phase 6) is kernel-mode Doom on top of the existing
`examples/kernel/` scaffolding.

## Layout

```
examples/doom/
  Makefile      # baseline all-C build (uses DOOM_SRC)
  README.md     # this file
  src/          # (future) ported .qd files live here
  wads/         # drop doom1.wad (shareware) here — not committed
```

## Prerequisites

- `sdl2-config`, `libSDL2`, `libSDL2_mixer` on the system.
- A local checkout of `github.com/ozkl/doomgeneric` for the C source.
  The Makefile expects it at `$HOME/dev/github/ozkl/doomgeneric/doomgeneric`;
  override with `make DOOM_SRC=/path/to/doomgeneric/doomgeneric`.
- `doom1.wad` (shareware Doom, freely redistributable) in `wads/`.
  Grab e.g. via `curl -L -o wads/doom1.wad \
  https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad`

## Building

```sh
make             # build ./build/doom
make run         # build and run with wads/doom1.wad
make clean
```

## Porting roadmap

See `/home/klarre/.claude/plans/bright-skipping-sunset.md` for the full plan.
Short version:

1. **Phase 0** (here): baseline all-C build runs.
2. **Phase 1**: add sized ints and packed structs to Quadrate.
3. **Phase 2**: write libc + SDL2 FFI shims.
4. **Phase 3**: port pure-data modules (tables, info, m_fixed, z_zone, …).
5. **Phase 4**: port subsystems bottom-up (WAD, platform, render, game).
6. **Phase 5**: drop libc shims as Quadrate stdlib catches up.
7. **Phase 6**: retarget `--freestanding`; VGA + PS/2 + PIT; kernel Doom.

As `.qd` replacements land, their corresponding `.c` files drop out of the
Makefile `SRCS` list.
