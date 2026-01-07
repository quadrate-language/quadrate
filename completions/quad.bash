# Bash completion for Quadrate toolchain
# Generated for quad, quadc, quadfmt, quadlint, quadlsp, quadpm, quadrepl, quaduses

# Helper function to find .qd files and directories
_quad_qd_files() {
    local cur="$1"
    compopt -o filenames
    COMPREPLY=( $(compgen -f -X '!*.qd' -- "$cur") $(compgen -d -- "$cur") )
}

# Main quad command completion
_quad() {
    local cur prev words cword
    _init_completion || return

    local commands="build run test fmt lint repl uses lsp help version"

    if [[ $cword -eq 1 ]]; then
        COMPREPLY=( $(compgen -W "$commands" -- "$cur") )
        return
    fi

    local cmd="${words[1]}"
    case "$cmd" in
        build)
            case "$prev" in
                -o)
                    COMPREPLY=( $(compgen -f -- "$cur") )
                    return
                    ;;
                -s|-l)
                    return
                    ;;
            esac
            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "-o -r --run -g -O0 -O1 -O2 -O3 -s -l --verbose --dump-tokens --dump-ir --save-temps --test --werror --help --version" -- "$cur") )
            else
                _quad_qd_files "$cur"
            fi
            ;;
        run)
            case "$prev" in
                -o)
                    COMPREPLY=( $(compgen -f -- "$cur") )
                    return
                    ;;
                -s|-l)
                    return
                    ;;
            esac
            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "-o -g -O0 -O1 -O2 -O3 -s -l --verbose --dump-tokens --dump-ir --save-temps --werror --help --version" -- "$cur") )
            else
                _quad_qd_files "$cur"
            fi
            ;;
        test)
            case "$prev" in
                -o)
                    COMPREPLY=( $(compgen -f -- "$cur") )
                    return
                    ;;
                -s|-l)
                    return
                    ;;
            esac
            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "-o -g -O0 -O1 -O2 -O3 -s -l --verbose --dump-tokens --dump-ir --save-temps --werror --help --version" -- "$cur") )
            else
                _quad_qd_files "$cur"
            fi
            ;;
        fmt)
            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "-c --check -w --write -n --help --version" -- "$cur") )
            else
                _quad_qd_files "$cur"
            fi
            ;;
        lint)
            case "$prev" in
                --max-nesting)
                    return
                    ;;
            esac
            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "--no-unused-functions --no-unused-variables --no-dead-code --no-deep-nesting --no-missing-defer --max-nesting --help --version" -- "$cur") )
            else
                _quad_qd_files "$cur"
            fi
            ;;
        uses)
            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "-w --write --help --version" -- "$cur") )
            else
                _quad_qd_files "$cur"
            fi
            ;;
        repl)
            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "-p --print --help --version" -- "$cur") )
            fi
            ;;
        lsp)
            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "--help --version" -- "$cur") )
            fi
            ;;
        help)
            COMPREPLY=( $(compgen -W "$commands" -- "$cur") )
            ;;
    esac
}

# quadc completion
_quadc() {
    local cur prev words cword
    _init_completion || return

    case "$prev" in
        -o)
            COMPREPLY=( $(compgen -f -- "$cur") )
            return
            ;;
        -s|-l)
            return
            ;;
    esac

    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "-h --help -v --version -o -r --run -g -O0 -O1 -O2 -O3 -s -l -I --verbose --dump-tokens --dump-ir --save-temps --test --werror" -- "$cur") )
    else
        _quad_qd_files "$cur"
    fi
}

# quadfmt completion
_quadfmt() {
    local cur prev words cword
    _init_completion || return

    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "-h --help -v --version -c --check -w --write" -- "$cur") )
    else
        _quad_qd_files "$cur"
    fi
}

# quadlint completion
_quadlint() {
    local cur prev words cword
    _init_completion || return

    case "$prev" in
        --max-nesting)
            return
            ;;
    esac

    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "-h --help -v --version --no-unused-functions --no-unused-variables --no-dead-code --no-deep-nesting --no-missing-defer --max-nesting" -- "$cur") )
    else
        _quad_qd_files "$cur"
    fi
}

# quadlsp completion
_quadlsp() {
    local cur prev words cword
    _init_completion || return

    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "-h --help -v --version" -- "$cur") )
    fi
}

# quadpm completion
_quadpm() {
    local cur prev words cword
    _init_completion || return

    local commands="install lock get update list build"

    if [[ $cword -eq 1 ]]; then
        if [[ "$cur" == -* ]]; then
            COMPREPLY=( $(compgen -W "-h --help -v --version" -- "$cur") )
        else
            COMPREPLY=( $(compgen -W "$commands" -- "$cur") )
        fi
        return
    fi

    local cmd="${words[1]}"
    case "$cmd" in
        install)
            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "--frozen" -- "$cur") )
            fi
            ;;
        get)
            # No completion for URLs
            ;;
        update)
            # Could complete installed module names, but that requires reading the modules dir
            ;;
        list|lock|build)
            # No arguments
            ;;
    esac
}

# quadrepl completion
_quadrepl() {
    local cur prev words cword
    _init_completion || return

    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "-h --help -v --version -p --print" -- "$cur") )
    fi
}

# quaduses completion
_quaduses() {
    local cur prev words cword
    _init_completion || return

    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "-h --help -v --version -w --write" -- "$cur") )
    else
        _quad_qd_files "$cur"
    fi
}

# Register completions
complete -F _quad quad
complete -F _quadc quadc
complete -F _quadfmt quadfmt
complete -F _quadlint quadlint
complete -F _quadlsp quadlsp
complete -F _quadpm quadpm
complete -F _quadrepl quadrepl
complete -F _quaduses quaduses
