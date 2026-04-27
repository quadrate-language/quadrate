# Reference frames for the renderer-calibration probe sweep

Phase 0 of the Quadrate-Doom-looks-like-C-Doom plan. The Quadrate
binary writes one PPM per canonical camera position via `make probe`
(see `qd/d_main.qd :: D_RunProbes` for the list). To measure how close
our render matches vanilla, this directory holds the same frames
captured from an externally-validated source port.

`tools/ppm_diff.py --sweep build/ ref/` produces the per-probe
similarity table.

## Recommended port: Chocolate Doom

Chocolate Doom replicates the original Doom renderer's pixel output
closely enough to be the de-facto "vanilla" reference among source
ports, and it's in `chaotic-aur`:

```
sudo pacman -S chocolate-doom
```

### Spawn-angle probes (easy — no demo authoring)

For probes that match a map's native player-1 spawn + default angle,
a straight warp + F1 screenshot is enough:

| Probe                              | Map   | Capture recipe                            |
|------------------------------------|-------|-------------------------------------------|
| `probe_01_e1m1_spawn_n.ppm`        | E1M1  | `chocolate-doom -iwad wads/doom1.wad -warp 1 1`, press F1 at once, quit |
| `probe_05_e1m5_spawn_n.ppm`        | E1M5  | `-warp 1 5` instead. (Player-1 spawn faces 90°.)                        |

Chocolate Doom writes screenshots as `DOOM0000.pcx` (or sequential
numbers). Convert each to PPM and drop into this directory:

```
chocolate-doom -iwad wads/doom1.wad -warp 1 1      # F1 on the spawn view, then quit
mv ~/.local/share/chocolate-doom/DOOM0000.pcx /tmp/shot.pcx
convert /tmp/shot.pcx -resize 320x200\! ref/probe_01_e1m1_spawn_n.ppm    # ImageMagick
```

Chocolate Doom defaults to a larger display; our probe PPMs are
320×200 paletted, so the `-resize 320x200!` nearest-neighbour step is
needed. If you run Chocolate Doom with `-2` (no upscaling) and tweak
`window_width=320` / `window_height=200` in `chocolate-doom.cfg`, the
screenshot will already be 320×200 and no resize is needed.

### Non-spawn probes (demo-authored)

Probes 03 / 06 / 07 (player at a pose other than the spawn) require a
short pre-recorded LMP that walks the player to the target pose. That
plumbing is not here yet — see the `probes-via-demos` TODO in
`tools/capture_ref.sh`. Skip those until Phase 1 needs them.

## Alternative: dsda-doom (more automation)

`dsda-doom` (also in `chaotic-aur`) is a PrBoom+ fork with
demo-driven screenshot automation. If you install it, you can drive
reference captures headlessly via `-auto_screenshot` + a synthetic
demo LMP for each probe. Again, that pipeline lives in
`tools/capture_ref.sh` and currently only implements the spawn-angle
subset.

## Layout

```
ref/
├── README.md                       (this file)
├── probe_01_e1m1_spawn_n.ppm       (manual, via chocolate-doom F1)
├── probe_05_e1m5_spawn_n.ppm       (manual, via chocolate-doom F1)
└── … further probes landed as capture automation grows.
```

## Success criterion per phase

Run `make probe-diff` after any renderer change. The `>5%` column of
`ppm_diff.py`'s sweep output should trend *down* as Phase 1-2-3 land.
Phase 0 baseline (with the current `r_perspective.qd` scaffold
renderer) is "very high" — that's the starting point we're trying to
walk down.
