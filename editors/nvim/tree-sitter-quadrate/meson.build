# Tree-sitter grammar tests for Quadrate

# Only run if tree-sitter is available
tree_sitter = find_program('tree-sitter', required: false)

if tree_sitter.found()
    # Test that runs tree-sitter tests
    test('tree-sitter-grammar',
        tree_sitter,
        args: ['test'],
        workdir: meson.current_source_dir(),
        suite: 'tree-sitter',
        timeout: 30
    )

    message('Tree-sitter tests enabled')
else
    warning('tree-sitter CLI not found, skipping tree-sitter grammar tests')
    warning('Install with: npm install -g tree-sitter-cli')
endif
