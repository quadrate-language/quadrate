quadrate_sources = files(
		'src/qd.cc',
		'src/runtime/runtime.c',
		'src/runtime/stack.c',
)

quadrate_inc = include_directories('include')

# Shared library
quadrate_shared = shared_library('quadrate',
		quadrate_sources,
		include_directories: quadrate_inc,
		install_rpath: '$ORIGIN',
		install: false
)

# Static library
quadrate_static = static_library('quadrate_static',
		quadrate_sources,
		include_directories: quadrate_inc,
		install: false
)

quadrate_dep = declare_dependency(
		link_with: quadrate_shared,
		include_directories: quadrate_inc
)

quadrate_static_dep = declare_dependency(
		link_with: quadrate_static,
		include_directories: quadrate_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
