# libinterp - Interpreted execution of Quadrate source
# Parses with lib/qc and walks the AST, calling lib/rt directly (no LLVM)

interp_sources = files(
		'src/interp.cc',
)

interp_inc = include_directories('include')

interp_lib = static_library('interp',
		interp_sources,
		include_directories: [interp_inc],
		dependencies: [qc_dep, qdrt_static_dep],
		install: false
)

interp_dep = declare_dependency(
		link_with: interp_lib,
		include_directories: interp_inc,
		dependencies: [qc_dep, qdrt_static_dep]
)

# Tests
if get_option('build_tests')
		subdir('tests')
endif
