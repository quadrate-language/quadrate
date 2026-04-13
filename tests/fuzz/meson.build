# Fuzzing targets for Quadrate
#
# Build fuzzing targets with:
#   meson setup build/fuzz --buildtype=debug -Dbuild_fuzz=true
#   meson compile -C build/fuzz
#
# Run fuzzer with:
#   ./build/fuzz/tests/fuzz/fuzz_parser tests/fuzz/corpus/ -max_len=10000

# Check if fuzzing is enabled and we're using clang
if not get_option('build_fuzz')
	subdir_done()
endif

cc = meson.get_compiler('cpp')
if cc.get_id() != 'clang'
	warning('Fuzzing requires clang compiler. Skipping fuzz targets.')
	subdir_done()
endif

# Fuzzing flags - need to instrument all code for coverage
fuzz_args = [
	'-fsanitize=fuzzer,address,undefined',
	'-fno-omit-frame-pointer',
	'-g',
	'-O1',  # Some optimization helps fuzzer performance
]

fuzz_link_args = [
	'-fsanitize=fuzzer,address,undefined',
]

# Get qc sources and includes for direct compilation with fuzzer instrumentation
qc_src_dir = meson.project_source_root() / 'lib' / 'qc' / 'src'
qc_inc = include_directories('../../lib/qc/include')

# u8t dependency
u8t_dep = subproject('u8t').get_variable('u8t_dep')

# Compile qc sources directly with fuzzer instrumentation
fuzz_qc_sources = files(
	qc_src_dir / 'ast.cc',
	qc_src_dir / 'ast_declarations.cc',
	qc_src_dir / 'ast_expressions.cc',
	qc_src_dir / 'ast_printer.cc',
	qc_src_dir / 'ast_statements.cc',
	qc_src_dir / 'ast_types.cc',
	qc_src_dir / 'colors.cc',
	qc_src_dir / 'error_reporter.cc',
	qc_src_dir / 'source_formatter.cc',
)

# Parser fuzz target - compiles qc sources with fuzzer instrumentation
fuzz_parser = executable('fuzz_parser',
	'fuzz_parser.cc',
	fuzz_qc_sources,
	include_directories: qc_inc,
	dependencies: [u8t_dep],
	cpp_args: fuzz_args,
	link_args: fuzz_link_args,
	build_by_default: true,
)

# Formatter fuzz target - checks idempotency and crash-freedom
fuzz_formatter = executable('fuzz_formatter',
	'fuzz_formatter.cc',
	fuzz_qc_sources,
	include_directories: qc_inc,
	dependencies: [u8t_dep],
	cpp_args: fuzz_args,
	link_args: fuzz_link_args,
	build_by_default: true,
)

# LSP text-position fuzz target - exercises position-based word lookup
# on arbitrary document text + line/character. Pure-text helper, no
# jansson or qc deps required.
fuzz_lsp_text = executable('fuzz_lsp_text',
	'fuzz_lsp_text.cc',
	files(meson.project_source_root() / 'cmd' / 'quadlsp' / 'src' / 'lsp_text.cc'),
	cpp_args: fuzz_args,
	link_args: fuzz_link_args,
	build_by_default: true,
)
