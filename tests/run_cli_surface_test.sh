#!/bin/bash

# Pins the CLI surface every Quadrate tool shares: the shape of --help, the
# format of --version, and how an unrecognised option is reported.
#
# These had drifted apart -- quadmcp accepted any flag silently and exited 0,
# quadpm answered with a capitalised "Error:" and dumped its whole help page to
# stdout, quadlint printed two contradictory lines for one bad option, and the
# help pages disagreed about column widths, section names and whether --no-color
# existed. lib/cli/src/help.cc renders the pages now, so the layout is computed
# rather than retyped; this file is what keeps a tool from opting back out.
#
# Usage: run_cli_surface_test.sh

set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

WORK_DIR=$(mktemp -d)
cleanup() { rm -rf "$WORK_DIR"; }
trap cleanup EXIT

BUILD_DIR="${BUILD_DIR:-build/debug}"

TOOLS="quad quadc quaddoc quadfmt quadlint quadlsp quadmcp quadpm quadrepl quaduses"

# quadmcp is compiled from .qd by the Makefile rather than by meson, so it only
# ever lands in dist/bin; the rest are taken from the build tree when it has
# them so the suite tests what was just built.
tool_path() {
    local name="$1"
    if [ -x "$PROJECT_ROOT/$BUILD_DIR/cmd/$name/$name" ]; then
        echo "$PROJECT_ROOT/$BUILD_DIR/cmd/$name/$name"
    else
        echo "$PROJECT_ROOT/dist/bin/$name"
    fi
}

for name in $TOOLS; do
    bin="$(tool_path "$name")"
    if [ ! -x "$bin" ]; then
        # Same U+2717 run_all.sh greps for, so the reason reaches the summary.
        echo -e "  ${RED}✗${NC} $name not found at $bin (not built? check for a skip warning)"
        exit 1
    fi
done

TESTS_RUN=0
TESTS_FAILED=0

pass() { echo -e "  ${GREEN}✓${NC} $1"; TESTS_RUN=$((TESTS_RUN + 1)); }
fail() {
    echo -e "  ${RED}✗${NC} $1"
    [ -n "${2:-}" ] && echo "      $2"
    TESTS_RUN=$((TESTS_RUN + 1))
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

check() {
    # check <name> <condition-result> <detail>
    if [ "$2" = "0" ]; then pass "$1"; else fail "$1" "$3"; fi
}

for name in $TOOLS; do
    bin="$(tool_path "$name")"
    echo ""
    echo "=== $name ==="

    # ---- --help ----
    help_out="$WORK_DIR/$name.help.out"
    help_err="$WORK_DIR/$name.help.err"
    rc=0
    "$bin" --help >"$help_out" 2>"$help_err" </dev/null || rc=$?

    check "--help exits 0" "$([ "$rc" -eq 0 ] && echo 0 || echo 1)" "exit status was $rc"
    check "--help writes nothing to stderr" "$([ ! -s "$help_err" ] && echo 0 || echo 1)" \
          "stderr: $(head -1 "$help_err")"
    check "--help writes to stdout" "$([ -s "$help_out" ] && echo 0 || echo 1)" "stdout was empty"

    # Title line: "<tool> - <summary>". The name has to be first so a reader
    # landing mid-scrollback knows which tool answered.
    first="$(head -1 "$help_out")"
    check "help opens with '$name - <summary>'" \
          "$(echo "$first" | grep -qE "^$name - .+[^.]$" && echo 0 || echo 1)" \
          "first line was: $first"

    check "help has a 'Usage: $name' line" \
          "$(grep -qE "^Usage: $name" "$help_out" && echo 0 || echo 1)" "no Usage: line"
    check "help has an Options: section" \
          "$(grep -qx "Options:" "$help_out" && echo 0 || echo 1)" "no Options: section"
    check "help has an Examples: section" \
          "$(grep -qx "Examples:" "$help_out" && echo 0 || echo 1)" "no Examples: section"

    # The three flags every tool takes, in one order, worded one way. Column
    # width is per-tool (it follows the widest flag), so the gap is a + not a
    # fixed count.
    trio="$(grep -E '^( {2}-h, --help| {2}-v, --version| {6}--no-color) ' "$help_out" | head -3)"
    expected=$'  -h, --help\n  -v, --version\n      --no-color'
    got="$(echo "$trio" | sed -E 's/ {2,}[A-Z].*$//')"
    check "Options opens with -h/-v/--no-color in order" \
          "$([ "$got" = "$expected" ] && echo 0 || echo 1)" "got: $(echo "$got" | tr '\n' '|')"

    check "--help describes --help the same way" \
          "$(grep -qE '^  -h, --help +Show this help message$' "$help_out" && echo 0 || echo 1)" \
          "wording or alignment differs"
    check "--help describes --version the same way" \
          "$(grep -qE '^  -v, --version +Show version information$' "$help_out" && echo 0 || echo 1)" \
          "wording or alignment differs"
    check "--help describes --no-color the same way" \
          "$(grep -qE '^      --no-color +Disable coloured output$' "$help_out" && echo 0 || echo 1)" \
          "wording or alignment differs"

    # Help is read through pipes and pagers as often as on a terminal.
    check "help contains no ANSI escapes" \
          "$(grep -qP '\x1b\[' "$help_out" && echo 1 || echo 0)" "found an escape sequence"

    # -h is the same page as --help, not a shorter one.
    "$bin" -h >"$WORK_DIR/$name.h.out" 2>/dev/null </dev/null || true
    check "-h matches --help" \
          "$(cmp -s "$help_out" "$WORK_DIR/$name.h.out" && echo 0 || echo 1)" "the two pages differ"

    # ---- --version ----
    ver_out="$WORK_DIR/$name.ver.out"
    ver_err="$WORK_DIR/$name.ver.err"
    rc=0
    "$bin" --version >"$ver_out" 2>"$ver_err" </dev/null || rc=$?
    check "--version exits 0" "$([ "$rc" -eq 0 ] && echo 0 || echo 1)" "exit status was $rc"
    check "--version writes nothing to stderr" "$([ ! -s "$ver_err" ] && echo 0 || echo 1)" \
          "stderr: $(head -1 "$ver_err")"
    # "<tool> <semver> (<git describe>, built <date>)" -- one line, one shape.
    check "--version matches the shared format" \
          "$(grep -qE "^$name [0-9]+\.[0-9]+\.[0-9]+.* \(.+, built .+\)$" "$ver_out" && echo 0 || echo 1)" \
          "got: $(head -1 "$ver_out")"
    check "--version is a single line" \
          "$([ "$(wc -l < "$ver_out")" -eq 1 ] && echo 0 || echo 1)" \
          "$(wc -l < "$ver_out") lines"

    "$bin" -v >"$WORK_DIR/$name.v.out" 2>/dev/null </dev/null || true
    check "-v matches --version" \
          "$(cmp -s "$ver_out" "$WORK_DIR/$name.v.out" && echo 0 || echo 1)" "the two differ"

    # ---- unknown option ----
    # The whole point: an option a tool does not know is an error, reported in
    # two lines on stderr with nothing on stdout. quadmcp used to ignore it and
    # start serving; quadpm used to print its entire help page to stdout.
    bad_out="$WORK_DIR/$name.bad.out"
    bad_err="$WORK_DIR/$name.bad.err"
    rc=0
    "$bin" --definitely-not-a-flag >"$bad_out" 2>"$bad_err" </dev/null || rc=$?
    check "unknown option exits 1" "$([ "$rc" -eq 1 ] && echo 0 || echo 1)" "exit status was $rc"
    check "unknown option writes nothing to stdout" \
          "$([ ! -s "$bad_out" ] && echo 0 || echo 1)" "stdout: $(head -1 "$bad_out")"
    check "unknown option names the tool and the flag" \
          "$(grep -qx "$name: unknown option: --definitely-not-a-flag" "$bad_err" && echo 0 || echo 1)" \
          "got: $(head -1 "$bad_err")"
    check "unknown option refers to --help" \
          "$(grep -qx "Try '$name --help' for more information." "$bad_err" && echo 0 || echo 1)" \
          "got: $(sed -n 2p "$bad_err")"
    check "unknown option says nothing else" \
          "$([ "$(wc -l < "$bad_err")" -eq 2 ] && echo 0 || echo 1)" \
          "$(wc -l < "$bad_err") lines on stderr"

    # ---- --no-color ----
    # Accepted everywhere, so a script can pass it to any tool without knowing
    # which ones actually colour their output.
    #
    # Run from an empty scratch directory: with no path argument quaddoc scans
    # the working directory and writes a site into ./docs, which from the
    # repository root means this check dropping ~100 generated .html files into
    # the tree it is testing.
    nc_err="$WORK_DIR/$name.nc.err"
    mkdir -p "$WORK_DIR/cwd"
    (cd "$WORK_DIR/cwd" && timeout 30 "$bin" --no-color >/dev/null 2>"$nc_err" </dev/null) || true
    check "--no-color is accepted" \
          "$(grep -q "unknown option" "$nc_err" && echo 1 || echo 0)" \
          "rejected: $(head -1 "$nc_err")"
done

# The bash completions are a second list of every tool's options, so they drift
# from --help the moment one is added. They had: no --no-color anywhere, no quadmcp
# at all, no `pm`/`mcp` under quad, and quadlint was missing ten of its flags.
echo ""
echo "=== completions match --help ==="

COMPLETIONS="$PROJECT_ROOT/completions/quad.bash"
if [ ! -f "$COMPLETIONS" ]; then
    fail "completions file exists" "$COMPLETIONS not found"
else
    pass "completions file exists"
    if bash -n "$COMPLETIONS" 2>/dev/null; then
        pass "completions parse"
    else
        fail "completions parse" "bash -n reported a syntax error"
    fi

    for name in $TOOLS; do
        bin="$(tool_path "$name")"
        missing=""
        # Long options as --help lists them, minus the "--" argument separator,
        # which is not something to complete.
        for opt in $("$bin" --help </dev/null 2>/dev/null |
                     grep -oE '^  (-[A-Za-z], )?--[a-z][a-z0-9-]*|^      --[a-z][a-z0-9-]*' |
                     grep -oE -- '--[a-z][a-z0-9-]*' | sort -u); do
            grep -qF -- "$opt" "$COMPLETIONS" || missing="$missing $opt"
        done
        if [ -z "$missing" ]; then
            pass "$name: every option in --help is completable"
        else
            fail "$name: every option in --help is completable" "missing:$missing"
        fi
    done

    # quad's command list, and one completion function per tool.
    for cmd in $("$(tool_path quad)" --help </dev/null 2>/dev/null |
                 sed -n '/^Commands:/,/^$/p' | grep -oE '^  [a-z]+' | tr -d ' '); do
        if grep -qE "^    local commands=\"[^\"]*\\b$cmd\\b" "$COMPLETIONS" ||
           grep -qE "commands=\"[^\"]* ?$cmd( |\")" "$COMPLETIONS"; then
            pass "quad: command '$cmd' is completable"
        else
            fail "quad: command '$cmd' is completable" "not in any commands= list"
        fi
    done

    for name in $TOOLS; do
        if grep -qx "complete -F _$name $name" "$COMPLETIONS"; then
            pass "$name: completion is registered"
        else
            fail "$name: completion is registered" "no 'complete -F _$name $name' line"
        fi
    done

    # Registering a completion in the file is not enough to get one. bash
    # loads a completion file lazily, by the name of the command being
    # completed, so one file serving ten tools needs a link per tool. The
    # install rule listed seven of the ten by hand: quaddoc and quadmcp had
    # working completions in the file that no shell would ever load.
fi

# The zsh completion is a second, independent list of every tool's options, with
# the same drift problem as the bash one. zsh needs no per-command links: the
# #compdef line names all ten commands and compinit indexes the file by them --
# which is exactly why that line has to stay complete.
echo ""
echo "=== zsh completions match --help ==="

ZCOMPLETIONS="$PROJECT_ROOT/completions/_quad"
if [ ! -f "$ZCOMPLETIONS" ]; then
    fail "zsh completion file exists" "$ZCOMPLETIONS not found"
else
    pass "zsh completion file exists"

    compdef_line=$(head -1 "$ZCOMPLETIONS")
    case "$compdef_line" in
        "#compdef "*) pass "zsh completion opens with #compdef" ;;
        *) fail "zsh completion opens with #compdef" "first line was: $compdef_line" ;;
    esac

    for name in $TOOLS; do
        case " $compdef_line " in
            *" $name "*) pass "$name: named on the #compdef line" ;;
            *) fail "$name: named on the #compdef line" \
                    "zsh will not load the file for $name" ;;
        esac
        # Every tool also needs a branch in the $service dispatch, or the file
        # loads and then completes nothing.
        if grep -qE "^\s+$name\)" "$ZCOMPLETIONS"; then
            pass "$name: has a \$service dispatch branch"
        else
            fail "$name: has a \$service dispatch branch" "no '$name)' case arm"
        fi
    done

    for name in $TOOLS; do
        missing=""
        for opt in $("$(tool_path "$name")" --help </dev/null 2>/dev/null |
                     grep -oE '^  (-[A-Za-z], )?--[a-z][a-z0-9-]*|^      --[a-z][a-z0-9-]*' |
                     grep -oE -- '--[a-z][a-z0-9-]*' | sort -u); do
            # Options are written either in full or as the long half of a
            # {-x,--long} pair, so match the bare name too.
            grep -qF -- "$opt" "$ZCOMPLETIONS" || missing="$missing $opt"
        done
        if [ -z "$missing" ]; then
            pass "$name: every option in --help is in the zsh completion"
        else
            fail "$name: every option in --help is in the zsh completion" "missing:$missing"
        fi
    done

    # zsh is not a build dependency, so only syntax-check when one is present.
    if command -v zsh >/dev/null 2>&1; then
        if zsh -n "$ZCOMPLETIONS" 2>/dev/null; then
            pass "zsh completion parses (zsh -n)"
        else
            fail "zsh completion parses (zsh -n)" "$(zsh -n "$ZCOMPLETIONS" 2>&1 | head -2)"
        fi
    else
        echo "  (zsh not installed; skipping the parse check)"
    fi
fi

# The fish completion is a third list of every tool's options. fish autoloads by
# command name as bash does, so it needs the same per-tool links.
echo ""
echo "=== fish completions match --help ==="

FCOMPLETIONS="$PROJECT_ROOT/completions/quad.fish"
if [ ! -f "$FCOMPLETIONS" ]; then
    fail "fish completion file exists" "$FCOMPLETIONS not found"
else
    pass "fish completion file exists"

    tools_line=$(grep -E '^set -l __quad_tools ' "$FCOMPLETIONS" || true)
    if [ -z "$tools_line" ]; then
        fail "fish completion lists its tools" "no '__quad_tools' line"
    else
        pass "fish completion lists its tools"
        for name in $TOOLS; do
            case " $tools_line " in
                *" $name "*) pass "$name: is in __quad_tools" ;;
                *) fail "$name: is in __quad_tools" \
                        "so it gets none of the shared options" ;;
            esac
        done
    fi

    for name in $TOOLS; do
        missing=""
        for opt in $("$(tool_path "$name")" --help </dev/null 2>/dev/null |
                     grep -oE '^  (-[A-Za-z], )?--[a-z][a-z0-9-]*|^      --[a-z][a-z0-9-]*' |
                     grep -oE -- '--[a-z][a-z0-9-]*' | sort -u); do
            # fish writes long options without their dashes: `-l no-color`.
            grep -qE -- "-l ${opt#--}( |\$)" "$FCOMPLETIONS" || missing="$missing $opt"
        done
        if [ -z "$missing" ]; then
            pass "$name: every option in --help is in the fish completion"
        else
            fail "$name: every option in --help is in the fish completion" "missing:$missing"
        fi
    done

    # fish is not a build dependency, so only syntax-check when one is present.
    if command -v fish >/dev/null 2>&1; then
        if fish -n "$FCOMPLETIONS" 2>/dev/null; then
            pass "fish completion parses (fish -n)"
        else
            fail "fish completion parses (fish -n)" "$(fish -n "$FCOMPLETIONS" 2>&1 | head -2)"
        fi
    else
        echo "  (fish not installed; skipping the parse check)"
    fi
fi

echo ""
echo "=== completions are installed where the shells look ==="
if true; then
    links=$(make -s -f "$PROJECT_ROOT/Makefile" -C "$PROJECT_ROOT" print-completion-links 2>/dev/null || true)
    if [ -z "$links" ]; then
        fail "Makefile exposes the completion link list" "print-completion-links produced nothing"
    else
        pass "Makefile exposes the completion link list"
        for name in $TOOLS; do
            # `quad` is the completion file itself, not a link to it.
            if [ "$name" = "quad" ]; then
                continue
            fi
            case " $links " in
                *" $name "*) pass "$name: completion is installed under its own name" ;;
                *) fail "$name: completion is installed under its own name" \
                        "not in COMPLETION_LINKS, so bash and fish will never load it for $name" ;;
            esac
        done

        # The install rule has to place all three files, not just the bash one.
        for spec in "bash-completion/completions/quad:bash" \
                    "zsh/site-functions/_quad:zsh" \
                    "fish/vendor_completions.d/quad.fish:fish"; do
            path="${spec%%:*}"; shell="${spec##*:}"
            if grep -qF "$path" "$PROJECT_ROOT/Makefile"; then
                pass "$shell completion has an install rule"
            else
                fail "$shell completion has an install rule" "no '$path' in the Makefile"
            fi
        done
    fi
fi

echo ""
if [ "$TESTS_FAILED" -gt 0 ]; then
    echo -e "${RED}FAILED${NC}: $TESTS_FAILED of $TESTS_RUN checks failed"
    exit 1
fi
echo -e "${GREEN}PASSED${NC}: all $TESTS_RUN checks passed"
exit 0
