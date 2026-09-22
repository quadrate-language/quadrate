# Bash completion for the Quadrate toolchain
# Covers quad, quadc, quaddoc, quadfmt, quadlint, quadlsp, quadmcp, quadpm,
# quadrepl and quaduses.
#
# Option lists here mirror each tool's --help. When you add an option to a tool,
# add it here too -- `<tool> --help` is the authoritative list.

# The three options every Quadrate tool accepts.
_QUAD_COMMON="-h --help -v --version --no-color"

# Per-tool options, without the common three.
_QUAD_OPTS_QUADC="-o --output -O0 -O1 -O2 -O3 -g --debug -s --stack-size -I --include -l --module -r --run --no-jit --test --coverage --target --freestanding --werror --verbose --save-temps --dump-tokens --dump-ast --dump-ir"
_QUAD_OPTS_QUADFMT="-c --check -w --write --no-sort-imports"
_QUAD_OPTS_QUADLINT="--json -q --quiet --max-nesting --no-unused-functions --no-unused-variables --no-dead-code --no-deep-nesting --no-missing-defer --no-shadow-variables --no-empty-blocks --no-constant-conditions --check-magic-numbers --check-long-functions --check-naming --max-function-lines"
_QUAD_OPTS_QUADUSES="-w --write -c --check -n --dry-run"
_QUAD_OPTS_QUADDOC="-o --output -q --quiet --title --css"
_QUAD_OPTS_QUADREPL="-p --print"
_QUAD_OPTS_QUADMCP="--http --host"
_QUAD_OPTS_QUADPM="--frozen --no-scripts"

# Helper function to find .qd files and directories
_quad_qd_files() {
    local cur="$1"
    compopt -o filenames
    COMPREPLY=( $(compgen -f -X '!*.qd' -- "$cur") $(compgen -d -- "$cur") )
}

# Options that take a value, so completion offers the value rather than a flag.
# Returns 0 when it handled $prev.
_quad_option_value() {
    local prev="$1" cur="$2"
    case "$prev" in
        -o|--output)
            COMPREPLY=( $(compgen -f -- "$cur") )
            return 0
            ;;
        --css)
            COMPREPLY=( $(compgen -f -- "$cur") )
            return 0
            ;;
        -I|--include)
            COMPREPLY=( $(compgen -d -- "$cur") )
            return 0
            ;;
        -s|--stack-size|-l|--module|--target|--title|--max-nesting|--max-function-lines)
            return 0
            ;;
    esac
    return 1
}

# Main quad command completion
_quad() {
    local cur prev words cword
    _init_completion || return

    local commands="build run test fmt lint repl uses lsp doc pm mcp init clean help version"

    if [[ $cword -eq 1 ]]; then
        if [[ "$cur" == -* ]]; then
            COMPREPLY=( $(compgen -W "$_QUAD_COMMON" -- "$cur") )
        else
            COMPREPLY=( $(compgen -W "$commands" -- "$cur") )
        fi
        return
    fi

    local cmd="${words[1]}"

    _quad_option_value "$prev" "$cur" && return

    local opts=""
    local takes_files=1
    case "$cmd" in
        build|run|test) opts="$_QUAD_OPTS_QUADC" ;;
        fmt)            opts="$_QUAD_OPTS_QUADFMT" ;;
        lint)           opts="$_QUAD_OPTS_QUADLINT" ;;
        uses)           opts="$_QUAD_OPTS_QUADUSES" ;;
        doc)            opts="$_QUAD_OPTS_QUADDOC"; takes_files=0 ;;
        repl)           opts="$_QUAD_OPTS_QUADREPL"; takes_files=0 ;;
        mcp)            opts="$_QUAD_OPTS_QUADMCP"; takes_files=0 ;;
        lsp)            opts=""; takes_files=0 ;;
        pm)
            _quadpm_args 2
            return
            ;;
        init|clean|version)
            takes_files=0
            ;;
        help)
            COMPREPLY=( $(compgen -W "$commands" -- "$cur") )
            return
            ;;
        *)
            return
            ;;
    esac

    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$_QUAD_COMMON $opts" -- "$cur") )
    elif [[ $takes_files -eq 1 ]]; then
        _quad_qd_files "$cur"
    elif [[ "$cmd" == "doc" ]]; then
        COMPREPLY=( $(compgen -d -- "$cur") )
    fi
}

# Shared body for quadpm and `quad pm`; $1 is the index its subcommand sits at.
_quadpm_args() {
    local base="$1"
    local commands="install lock get update remove list outdated build"

    if [[ $cword -eq $base ]]; then
        if [[ "$cur" == -* ]]; then
            COMPREPLY=( $(compgen -W "$_QUAD_COMMON" -- "$cur") )
        else
            COMPREPLY=( $(compgen -W "$commands" -- "$cur") )
        fi
        return
    fi

    case "${words[$base]}" in
        install|i)
            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "$_QUAD_COMMON $_QUAD_OPTS_QUADPM" -- "$cur") )
            fi
            ;;
        get)
            # A Git URL; nothing useful to offer.
            ;;
        update|remove|rm|uninstall)
            # Could complete installed module names, but that means reading the
            # modules directory on every Tab.
            ;;
        *)
            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "$_QUAD_COMMON" -- "$cur") )
            fi
            ;;
    esac
}

_quadc() {
    local cur prev words cword
    _init_completion || return
    _quad_option_value "$prev" "$cur" && return
    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$_QUAD_COMMON $_QUAD_OPTS_QUADC" -- "$cur") )
    else
        _quad_qd_files "$cur"
    fi
}

_quadfmt() {
    local cur prev words cword
    _init_completion || return
    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$_QUAD_COMMON $_QUAD_OPTS_QUADFMT" -- "$cur") )
    else
        _quad_qd_files "$cur"
    fi
}

_quadlint() {
    local cur prev words cword
    _init_completion || return
    _quad_option_value "$prev" "$cur" && return
    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$_QUAD_COMMON $_QUAD_OPTS_QUADLINT" -- "$cur") )
    else
        _quad_qd_files "$cur"
    fi
}

_quaduses() {
    local cur prev words cword
    _init_completion || return
    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$_QUAD_COMMON $_QUAD_OPTS_QUADUSES" -- "$cur") )
    else
        _quad_qd_files "$cur"
    fi
}

_quaddoc() {
    local cur prev words cword
    _init_completion || return
    _quad_option_value "$prev" "$cur" && return
    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$_QUAD_COMMON $_QUAD_OPTS_QUADDOC" -- "$cur") )
    else
        COMPREPLY=( $(compgen -d -- "$cur") )
    fi
}

_quadlsp() {
    local cur prev words cword
    _init_completion || return
    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$_QUAD_COMMON" -- "$cur") )
    fi
}

_quadrepl() {
    local cur prev words cword
    _init_completion || return
    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$_QUAD_COMMON $_QUAD_OPTS_QUADREPL" -- "$cur") )
    fi
}

_quadmcp() {
    local cur prev words cword
    _init_completion || return
    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$_QUAD_COMMON $_QUAD_OPTS_QUADMCP" -- "$cur") )
    fi
}

_quadpm() {
    local cur prev words cword
    _init_completion || return
    _quadpm_args 1
}

# Register completions
complete -F _quad quad
complete -F _quadc quadc
complete -F _quaddoc quaddoc
complete -F _quadfmt quadfmt
complete -F _quadlint quadlint
complete -F _quadlsp quadlsp
complete -F _quadmcp quadmcp
complete -F _quadpm quadpm
complete -F _quadrepl quadrepl
complete -F _quaduses quaduses
