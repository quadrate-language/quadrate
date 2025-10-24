cgen_sources = files(
		'src/compiler.cc',
		'src/linker.cc',
		'src/transpiler.cc',
)

cgen_inc = include_directories('include')

cgen_lib = static_library('cgen',
		cgen_sources,
		include_directories: cgen_inc,
		dependencies: qc_dep,
		install: false
)

cgen_dep = declare_dependency(
		link_with: cgen_lib,
		include_directories: cgen_inc,
		dependencies: qc_dep
)

# Tests
if get_option('build_tests')
		subdir('tests')
endif
