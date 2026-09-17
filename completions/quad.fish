# Fish completion for the Quadrate toolchain.
#
# One file serves all ten tools. fish autoloads a completion file by the name of
# the command being completed, as bash-completion does, so `make install` links
# <tool>.fish to this file for each tool; the definitions below name their
# command explicitly with `complete -c`, so it does not matter which name was
# loaded.
#
# Option lists mirror each tool's --help; `<tool> --help` is the authoritative
# list, and tests/run_cli_surface_test.sh fails if an option documented there is
# missing from here.

set -l __quad_tools quad quadc quaddoc quadfmt quadlint quadlsp quadmcp quadpm quadrepl quaduses

# Files with a given suffix, plus directories so they can be descended into.
#
# Deliberately not __fish_complete_suffix: that helper sorts matching files
# first but still returns every file, so `quadfmt <TAB>` would offer
# CHANGELOG.md and `--css <TAB>` would offer a .txt. Ask fish for ordinary file
# completions for the token typed so far and keep only what the option can
# actually take, which is what the bash and zsh completions offer.
function __quad_files_matching --description 'Files with a suffix, plus directories'
    set -l token (commandline -ct | string replace -r -- '^-[^=]*=' '')
    set -l suffix (string escape --style=regex -- $argv[1])
    complete -C "__fish_command_without_completions $token" | string match -r -- '^.*'$suffix'$|^.*/$'
end

function __quad_qd_files --description 'Quadrate sources and directories'
    __quad_files_matching .qd
end

# The three options every Quadrate tool accepts.
for cmd in $__quad_tools
    complete -c $cmd -s h -l help -d 'Show this help message'
    complete -c $cmd -s v -l version -d 'Show version information'
    complete -c $cmd -l no-color -d 'Disable coloured output'
end

# Tools that take no file operand at all.
for cmd in quadlsp quadrepl quadmcp
    complete -c $cmd -f
end

# --- quadc ------------------------------------------------------------------
complete -c quadc -f -a '(__quad_qd_files)'
complete -c quadc -s o -l output -r -F -d 'Output executable name'
complete -c quadc -o O0 -d 'No optimization (default)'
complete -c quadc -o O1 -d 'Optimize a little'
complete -c quadc -o O2 -d 'Optimize more'
complete -c quadc -o O3 -d 'Optimize most'
complete -c quadc -s g -l debug -d 'Generate debug information for GDB/LLDB'
complete -c quadc -s s -l stack-size -x -d 'Set stack size'
complete -c quadc -s I -l include -x -a '(__fish_complete_directories)' -d 'Add a module search path'
complete -c quadc -s l -l module -x -d 'Pin a module to a version (module@version)'
complete -c quadc -s r -l run -d 'Compile and run immediately'
complete -c quadc -l no-jit -d 'Link and execute instead of using the JIT'
complete -c quadc -l test -d 'Compile and run tests'
complete -c quadc -l coverage -d 'Print a function coverage report'
complete -c quadc -l target -x -d 'Cross-compile for a target triple'
complete -c quadc -l freestanding -d 'No hosted runtime: no libc, no auto-main'
complete -c quadc -l werror -d 'Treat warnings as errors'
complete -c quadc -l verbose -d 'Show detailed compilation steps'
complete -c quadc -l save-temps -d 'Keep temporary files for debugging'
complete -c quadc -l dump-tokens -d 'Print the lexer token stream'
complete -c quadc -l dump-ast -d 'Print the parsed AST'
complete -c quadc -l dump-ir -d 'Print the generated LLVM IR'

# --- quadfmt ----------------------------------------------------------------
complete -c quadfmt -f -a '(__quad_qd_files)'
complete -c quadfmt -s c -l check -d 'Report whether files are formatted (exit 1 if not)'
complete -c quadfmt -s w -l write -d 'Format files in place'
complete -c quadfmt -l no-sort-imports -d 'Leave use statements in their original order'

# --- quadlint ---------------------------------------------------------------
complete -c quadlint -f -a '(__quad_qd_files)'
complete -c quadlint -l json -d 'Report issues as JSON (for editors and CI)'
complete -c quadlint -s q -l quiet -d 'Only show the summary'
complete -c quadlint -l max-nesting -x -d 'Maximum nesting depth'
complete -c quadlint -l no-unused-functions -d 'Disable unused function warnings'
complete -c quadlint -l no-unused-variables -d 'Disable unused variable warnings'
complete -c quadlint -l no-dead-code -d 'Disable dead code warnings'
complete -c quadlint -l no-deep-nesting -d 'Disable deep nesting warnings'
complete -c quadlint -l no-missing-defer -d 'Disable missing defer warnings'
complete -c quadlint -l no-shadow-variables -d 'Disable shadow variable warnings'
complete -c quadlint -l no-empty-blocks -d 'Disable empty block warnings'
complete -c quadlint -l no-constant-conditions -d 'Disable constant condition warnings'
complete -c quadlint -l check-magic-numbers -d 'Enable magic number detection'
complete -c quadlint -l check-long-functions -d 'Enable long function detection'
complete -c quadlint -l check-naming -d 'Enable naming convention checks'
complete -c quadlint -l max-function-lines -x -d 'Maximum function lines'

# --- quaduses ---------------------------------------------------------------
complete -c quaduses -f -a '(__quad_qd_files)'
complete -c quaduses -s w -l write -d 'Update files in place'
complete -c quaduses -s c -l check -d 'Report whether files need changes (exit 1 if so)'
complete -c quaduses -s n -l dry-run -d 'Show what would change without modifying'

# --- quaddoc ----------------------------------------------------------------
complete -c quaddoc -f -a '(__fish_complete_directories)'
complete -c quaddoc -s o -l output -x -a '(__fish_complete_directories)' -d 'Output directory'
complete -c quaddoc -s q -l quiet -d 'Print nothing but errors'
complete -c quaddoc -l title -x -d 'Project title'
complete -c quaddoc -l css -x -a '(__quad_files_matching .css)' -d 'Append a custom CSS file'

# --- quadrepl ---------------------------------------------------------------
complete -c quadrepl -s p -l print -d 'Print stack to stdout on exit'

# --- quadmcp ----------------------------------------------------------------
complete -c quadmcp -l http -d 'Serve over HTTP on :3000 instead of stdio'

# --- quadpm -----------------------------------------------------------------
complete -c quadpm -f
complete -c quadpm -n __fish_use_subcommand -a install -d 'Install the dependencies listed in qd.json'
complete -c quadpm -n __fish_use_subcommand -a lock -d 'Generate or update qd.lock from the installed modules'
complete -c quadpm -n __fish_use_subcommand -a get -d 'Fetch and install one module from Git'
complete -c quadpm -n __fish_use_subcommand -a update -d 'Update installed modules (git pull)'
complete -c quadpm -n __fish_use_subcommand -a remove -d 'Remove an installed module'
complete -c quadpm -n __fish_use_subcommand -a list -d 'List installed modules'
complete -c quadpm -n __fish_use_subcommand -a outdated -d 'Show modules with newer versions available'
complete -c quadpm -n __fish_use_subcommand -a build -d 'Build the C sources of the module in this directory'
complete -c quadpm -n '__fish_seen_subcommand_from install' -l frozen \
    -d 'Install only from qd.lock (fail if outdated)'
complete -c quadpm -n '__fish_seen_subcommand_from install get update build' -l no-scripts \
    -d 'Do not run modules\' prebuild scripts'

# --- quad -------------------------------------------------------------------
# The dispatcher. Each command runs the matching tool and passes options
# straight through, so the conditions below re-offer that tool's options.
complete -c quad -f
complete -c quad -n __fish_use_subcommand -a build -d 'Compile Quadrate source files'
complete -c quad -n __fish_use_subcommand -a run -d 'Build and run a Quadrate program'
complete -c quad -n __fish_use_subcommand -a test -d 'Run tests'
complete -c quad -n __fish_use_subcommand -a fmt -d 'Format Quadrate source files'
complete -c quad -n __fish_use_subcommand -a lint -d 'Check code for common issues'
complete -c quad -n __fish_use_subcommand -a repl -d 'Start interactive REPL'
complete -c quad -n __fish_use_subcommand -a uses -d 'Manage use statements'
complete -c quad -n __fish_use_subcommand -a lsp -d 'Start language server'
complete -c quad -n __fish_use_subcommand -a doc -d 'Generate HTML documentation'
complete -c quad -n __fish_use_subcommand -a pm -d 'Manage third-party modules'
complete -c quad -n __fish_use_subcommand -a mcp -d 'Start the MCP server'
complete -c quad -n __fish_use_subcommand -a init -d 'Initialize a new Quadrate project'
complete -c quad -n __fish_use_subcommand -a clean -d 'Remove build artifacts'
complete -c quad -n __fish_use_subcommand -a help -d 'Show help for a command'
complete -c quad -n __fish_use_subcommand -a version -d 'Show version information'
# `quad script.qd` runs a script directly, for shebang lines.
complete -c quad -n __fish_use_subcommand -a '(__quad_qd_files)'

complete -c quad -n '__fish_seen_subcommand_from help' \
    -a 'build run test fmt lint repl uses lsp doc pm mcp init clean version'

# Pass-through: offer what the underlying tool accepts.
complete -c quad -n '__fish_seen_subcommand_from build run test' -a '(__quad_qd_files)'
complete -c quad -n '__fish_seen_subcommand_from build run test' -s o -l output -r -F -d 'Output executable name'
complete -c quad -n '__fish_seen_subcommand_from build run test' -s g -l debug -d 'Generate debug information'
complete -c quad -n '__fish_seen_subcommand_from build run test' -s r -l run -d 'Compile and run immediately'
complete -c quad -n '__fish_seen_subcommand_from build run test' -l verbose -d 'Show detailed compilation steps'
complete -c quad -n '__fish_seen_subcommand_from build run test' -l werror -d 'Treat warnings as errors'
complete -c quad -n '__fish_seen_subcommand_from build run test' -l coverage -d 'Print a function coverage report'
complete -c quad -n '__fish_seen_subcommand_from build run test' -l freestanding \
    -d 'No hosted runtime: no libc, no auto-main'
complete -c quad -n '__fish_seen_subcommand_from build run test' -l target -x -d 'Cross-compile for a target triple'

complete -c quad -n '__fish_seen_subcommand_from fmt' -a '(__quad_qd_files)'
complete -c quad -n '__fish_seen_subcommand_from fmt' -s c -l check -d 'Report whether files are formatted'
complete -c quad -n '__fish_seen_subcommand_from fmt' -s w -l write -d 'Format files in place'
complete -c quad -n '__fish_seen_subcommand_from fmt' -l no-sort-imports -d 'Leave use statements in order'

complete -c quad -n '__fish_seen_subcommand_from lint' -a '(__quad_qd_files)'
complete -c quad -n '__fish_seen_subcommand_from lint' -l json -d 'Report issues as JSON'
complete -c quad -n '__fish_seen_subcommand_from lint' -s q -l quiet -d 'Only show the summary'

complete -c quad -n '__fish_seen_subcommand_from uses' -a '(__quad_qd_files)'
complete -c quad -n '__fish_seen_subcommand_from uses' -s w -l write -d 'Update files in place'
complete -c quad -n '__fish_seen_subcommand_from uses' -s c -l check -d 'Report whether files need changes'
complete -c quad -n '__fish_seen_subcommand_from uses' -s n -l dry-run -d 'Show what would change'

complete -c quad -n '__fish_seen_subcommand_from doc' -a '(__fish_complete_directories)'
complete -c quad -n '__fish_seen_subcommand_from doc' -s o -l output -r \
    -a '(__fish_complete_directories)' -d 'Output directory'
complete -c quad -n '__fish_seen_subcommand_from doc' -s q -l quiet -d 'Print nothing but errors'
complete -c quad -n '__fish_seen_subcommand_from doc' -l title -x -d 'Project title'
complete -c quad -n '__fish_seen_subcommand_from doc' -l css -x -a '(__quad_files_matching .css)' \
    -d 'Append a custom CSS file'

complete -c quad -n '__fish_seen_subcommand_from repl' -s p -l print -d 'Print stack to stdout on exit'
complete -c quad -n '__fish_seen_subcommand_from mcp' -l http -d 'Serve over HTTP on :3000 instead of stdio'

complete -c quad -n '__fish_seen_subcommand_from pm' \
    -a 'install lock get update remove list outdated build'
complete -c quad -n '__fish_seen_subcommand_from pm' -l frozen -d 'Install only from qd.lock'
complete -c quad -n '__fish_seen_subcommand_from pm' -l no-scripts -d 'Do not run prebuild scripts'
