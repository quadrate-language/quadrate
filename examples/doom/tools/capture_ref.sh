#!/usr/bin/env bash
# Capture reference frames for the renderer-calibration probe sweep
# from an externally-validated source port (chocolate-doom preferred,
# dsda-doom / crispy-doom accepted). Writes PPM files into ref/ with
# the canonical probe_NN_...ppm names that tools/ppm_diff.py expects.
#
# Scope: today, only the spawn-angle probes (01, 05) are fully
# automated. Probes that need the player at a non-spawn pose require a
# synthetic LMP demo; that pipeline is a TODO at the bottom of this
# script (guarded by `PROBES_VIA_DEMOS`).
#
# Usage:
#   tools/capture_ref.sh check            # report which port is installed
#   tools/capture_ref.sh spawn            # capture probe_01, probe_05
#   tools/capture_ref.sh convert PCX OUT  # one-off PCX/PNG → PPM helper

set -euo pipefail

DOOM_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IWAD="${DOOM_DIR}/wads/doom1.wad"
REF_DIR="${DOOM_DIR}/ref"

detect_port() {
	for p in chocolate-doom crispy-doom dsda-doom prboom-plus; do
		if command -v "$p" >/dev/null 2>&1; then
			echo "$p"
			return 0
		fi
	done
	return 1
}

require_imagemagick() {
	if ! command -v magick >/dev/null 2>&1 && ! command -v convert >/dev/null 2>&1; then
		echo "error: ImageMagick not found (needed to convert PCX/PNG → PPM)" >&2
		echo "       install with: sudo pacman -S imagemagick" >&2
		exit 1
	fi
}

# Run the ImageMagick CLI via whichever entry point is installed.
im() {
	if command -v magick >/dev/null 2>&1; then
		magick "$@"
	else
		convert "$@"
	fi
}

# PCX (chocolate-doom) or PNG (dsda-doom, crispy-doom) → 320×200 PPM.
convert_to_ppm() {
	local src="$1"
	local dst="$2"
	require_imagemagick
	im "$src" -filter point -resize '320x200!' "$dst"
	echo "wrote $dst"
}

cmd_check() {
	if port="$(detect_port)"; then
		echo "found source port: $port"
		"$port" --version 2>&1 | head -1 || true
	else
		cat >&2 <<-EOF
			no vanilla-faithful source port installed. install one of:
			  sudo pacman -S chocolate-doom        (preferred)
			  sudo pacman -S crispy-doom           (chocolate fork, limit-removing)
			  sudo pacman -S dsda-doom             (PrBoom+ fork, best automation)
			then re-run this script.
		EOF
		return 1
	fi
}

# Interactive spawn-angle capture. Drives chocolate-doom or crispy-doom
# at 320×200 so the F1 screenshot lands the right resolution for us,
# then points the user at the saved PCX/PNG and runs the conversion.
cmd_spawn() {
	local port
	port="$(detect_port)"

	case "$port" in
		chocolate-doom|crispy-doom)
			ext="pcx"
			cfg_note="chocolate-doom.cfg: set window_width=320, window_height=200 for pixel-accurate screenshots"
			;;
		dsda-doom|prboom-plus)
			ext="png"
			cfg_note=""
			;;
		*) echo "unsupported port: $port" >&2; return 1 ;;
	esac

	cat <<-EOF
		==================================================================
		Capturing E1M1 + E1M5 spawn-angle reference frames with '$port'.

		$cfg_note

		For each map below, the port will launch; press F1 on the first
		frame to screenshot, then quit (q / Esc → quit). The script then
		converts the screenshot to 320×200 PPM and drops it in ref/.
		==================================================================

	EOF

	for p in \
		'1 1:probe_01_e1m1_spawn_n' \
		'1 5:probe_05_e1m5_spawn_n'
	do
		warp="${p%%:*}"
		name="${p##*:}"
		shot_dir="${HOME}/.local/share/$port"
		tmp_shot="$(mktemp -d)/shot.$ext"

		echo "-- probe '$name': launching $port -warp $warp"
		echo "   screenshot via F1, then quit."
		rm -f "$shot_dir"/DOOM????.pcx "$shot_dir"/doom????.png 2>/dev/null || true
		"$port" -iwad "$IWAD" -warp $warp || true

		# Grab the newest screenshot the port wrote, whatever its name.
		latest="$(ls -t "$shot_dir"/DOOM????.pcx "$shot_dir"/doom????.png 2>/dev/null | head -1 || true)"
		if [[ -z "${latest:-}" ]]; then
			echo "   no screenshot found in $shot_dir — skipped"
			continue
		fi
		cp -- "$latest" "$tmp_shot"
		convert_to_ppm "$tmp_shot" "$REF_DIR/$name.ppm"
	done
}

cmd_convert() {
	local src="${1:?usage: capture_ref.sh convert <src> <dst>}"
	local dst="${2:?usage: capture_ref.sh convert <src> <dst>}"
	convert_to_ppm "$src" "$dst"
}

case "${1:-}" in
	check)   cmd_check ;;
	spawn)   cmd_spawn ;;
	convert) shift; cmd_convert "$@" ;;
	*)
		cat >&2 <<-EOF
			usage: $0 <command>
			  check            report which source port is installed
			  spawn            capture probe_01 + probe_05 from the E1M1 / E1M5 spawns
			  convert SRC DST  convert a single PCX/PNG screenshot to 320×200 PPM

			reference pipeline for non-spawn probes (probe_03/06/07) isn't wired up
			yet — it needs a synthetic LMP demo authored per-probe. Grep
			'PROBES_VIA_DEMOS' in this file for where to plug it in.
		EOF
		exit 2
		;;
esac

# TODO(PROBES_VIA_DEMOS):
#  Demo-authored probe capture. For each non-spawn probe, build a 13-
#  byte Doom-1.9 header + a minimal ticcmd stream that walks the
#  player from the map's spawn to the probe pose, followed by the
#  0x80 terminator. Feed through `dsda-doom -iwad ... -playdemo ...
#  -auto_screenshot <N>,ref/probe_NN` where N is the last tic. Not
#  started — comfortable to add once Phase 1 needs those probes for
#  calibration. See https://github.com/kraflab/dsda-doom — look for
#  the `-auto_screenshot` option in `doc/cli.md`.
