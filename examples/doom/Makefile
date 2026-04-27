# doom - Port of doomgeneric (Doom) to Quadrate
#
# Phase 0: baseline all-C SDL2 build, to confirm the vendored C source runs
# before any Quadrate porting begins. Subsequent phases will replace .c files
# with .qd files one subsystem at a time.
#
# Requires sdl2-config and libSDL2_mixer on PATH.
#
# DOOM_SRC points at the doomgeneric C source tree. Override on the command
# line until we decide whether to vendor-in or git-submodule the source.
DOOM_SRC ?= $(HOME)/dev/github/ozkl/doomgeneric/doomgeneric

ifeq ($(V),1)
	VB=
else
	VB=@
endif

CC      ?= cc
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS   = $(shell sdl2-config --libs) -lSDL2_mixer

CFLAGS  += -std=gnu11 -O2 -g -Wall -DNORMALUNIX -DLINUX -DFEATURE_SOUND $(SDL_CFLAGS)
LDLIBS  += -lm $(SDL_LIBS)

BUILDDIR = build
OUT      = $(BUILDDIR)/doom

# Source list lifted from $(DOOM_SRC)/Makefile.sdl.
SRCS = \
	dummy.c am_map.c doomdef.c doomstat.c dstrings.c \
	d_event.c d_items.c d_iwad.c d_loop.c d_main.c d_mode.c d_net.c \
	f_finale.c f_wipe.c g_game.c hu_lib.c hu_stuff.c info.c \
	i_cdmus.c i_endoom.c i_joystick.c i_scale.c i_sound.c i_system.c i_timer.c \
	memio.c m_argv.c m_bbox.c m_cheat.c m_config.c m_controls.c m_fixed.c \
	m_menu.c m_misc.c m_random.c \
	p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c \
	p_map.c p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c p_setup.c \
	p_sight.c p_spec.c p_switch.c p_telept.c p_tick.c p_user.c \
	r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c \
	sha1.c sounds.c statdump.c st_lib.c st_stuff.c s_sound.c \
	tables.c v_video.c wi_stuff.c \
	w_checksum.c w_file.c w_main.c w_wad.c z_zone.c w_file_stdc.c \
	i_input.c i_video.c \
	doomgeneric.c doomgeneric_sdl.c \
	mus2mid.c i_sdlmusic.c i_sdlsound.c

OBJS = $(patsubst %.c,$(BUILDDIR)/%.o,$(SRCS))

all: $(OUT) ffi hello-sdl palette-demo titlepic-demo qd-tests quad-doom

# Build the Quadrate-side SDL shim (libdoomsdl.a installed into dist/lib).
.PHONY: ffi
ffi:
	$(MAKE) -C ffi

# Quadrate sanity test — builds only after ffi is ready.
QUADC      = $(realpath ../../dist/bin/quadc)
QUAD_ROOT  = $(realpath ../../dist/share/quadrate)
.PHONY: hello-sdl
hello-sdl: ffi | $(BUILDDIR)
	@echo "[QD  ] hello_sdl.qd"
	$(VB)QUADRATE_ROOT=$(QUAD_ROOT) $(QUADC) hello_sdl.qd -o $(BUILDDIR)/hello_sdl

# End-to-end demo: opens an SDL window, loads PLAYPAL from wads/doom1.wad,
# renders a 16×16 palette grid. Exercises every Phase 1-4 feature —
# sized ints, @packed structs, module-level vars, FFI, mem accessors.
.PHONY: palette-demo
palette-demo: ffi | $(BUILDDIR)
	@echo "[QD  ] palette_demo.qd"
	$(VB)QUADRATE_ROOT=$(QUAD_ROOT) $(QUADC) palette_demo.qd -o $(BUILDDIR)/palette_demo

# Renders TITLEPIC from doom1.wad — our first recognisable Doom art on
# screen. Decodes the column-run patch format via qd/v_video.qd, palette-maps
# through PLAYPAL, upscales 2× into a 640×400 SDL window, and dumps a PPM
# snapshot to build/titlepic.ppm for headless verification.
.PHONY: titlepic-demo
titlepic-demo: ffi | $(BUILDDIR)
	@echo "[QD  ] titlepic_demo.qd"
	$(VB)QUADRATE_ROOT=$(QUAD_ROOT) $(QUADC) titlepic_demo.qd -o $(BUILDDIR)/titlepic_demo

# Full 100% Quadrate Doom binary. Builds qd/doom.qd which is the entry
# point calling D_DoomMain — every Doom subsystem ported from
# doomgeneric lives in qd/ as a Quadrate module. The output binary
# links against libdoomsdl.a (the SDL FFI shim) and opens a 320×200
# SDL window.
.PHONY: quad-doom
quad-doom: ffi | $(BUILDDIR)
	@echo "[QD  ] qd/doom.qd"
	$(VB)QUADRATE_ROOT=$(QUAD_ROOT) $(QUADC) qd/doom.qd -o $(BUILDDIR)/doom

# Per-module round-trip tests for the ported Quadrate modules. Each _test.qd
# both compiles and runs; the runner checks output against a .expected file.
# Single recipe loops over QD_MODULES so pattern rules don't get confused by
# the mix of phony and file targets.
QD_MODULES = m_fixed m_random z_zone w_wad tables info m_argv dstrings sounds m_bbox m_misc m_controls doomstat memio m_cheat sha1 w_main i_timer i_system i_stubs d_mode sound d_iwad m_config i_input d_loop v_video_ext i_video r_draw doomgeneric r_sky r_data r_plane r_main r_bsp r_segs r_things p_tick p_pspr p_stubs ui_stubs g_game p_setup
.PHONY: qd-tests
qd-tests: ffi | $(BUILDDIR)
	@set -e; for m in $(QD_MODULES); do \
		echo "[QD  ] qd/$${m}_test.qd"; \
		QUADRATE_ROOT=$(QUAD_ROOT) $(QUADC) qd/$${m}_test.qd -o $(BUILDDIR)/$${m}_test; \
		SDL_VIDEODRIVER=dummy $(BUILDDIR)/$${m}_test > $(BUILDDIR)/$${m}_test.actual; \
		if [ -f qd/$${m}_test.expected ]; then \
			if diff -q qd/$${m}_test.expected $(BUILDDIR)/$${m}_test.actual >/dev/null; then \
				echo "[OK  ] qd/$${m}_test.qd"; \
			else \
				echo "[FAIL] qd/$${m}_test.qd — output differs"; \
				diff qd/$${m}_test.expected $(BUILDDIR)/$${m}_test.actual; \
				exit 1; \
			fi; \
		else \
			echo "[RUN ] qd/$${m}_test.qd (no .expected, exit-code only)"; \
		fi; \
	done

$(OUT): $(OBJS)
	@echo "[LINK] $@"
	$(VB)$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDLIBS)

$(BUILDDIR)/%.o: $(DOOM_SRC)/%.c | $(BUILDDIR)
	@echo "[CC  ] $<"
	$(VB)$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	$(VB)mkdir -p $(BUILDDIR)

run: $(OUT)
	$(OUT) -iwad wads/doom1.wad

run-game: $(OUT)
	$(OUT) -iwad wads/DOOM.WAD

# Headless scripted tour — dumps PPM snapshots at tics 0/15/30/45 + title +
# intermission. Useful for CI visual verification.
tour: $(OUT)
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(OUT) -tour

# Renderer calibration sweep. Dumps build/probe_*.ppm at a fixed set of
# (episode, map, camera) positions chosen to exercise interior walls,
# hallway depth, outdoor sky, and diagonal views. Compare against
# reference frames captured from the vanilla C Doom binary with
# tools/ppm_diff.py --sweep build/ ref/ .
probe: $(OUT)
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy $(OUT) -iwad wads/doom1.wad -probe

# Render the probe sweep then diff against ref/ — the main "how close
# are we to vanilla" signal. Prints 'no reference frames yet' instead
# of a Python error when ref/ is empty (Phase 0 baseline).
probe-diff: probe
	@if ls ref/probe_*.ppm >/dev/null 2>&1; then \
		tools/ppm_diff.py --sweep $(BUILDDIR) ref; \
	else \
		echo "no reference frames yet; see ref/README.md"; \
	fi

clean:
	rm -rf $(BUILDDIR)
	$(MAKE) -C ffi clean

.PHONY: all run tour probe probe-diff clean
