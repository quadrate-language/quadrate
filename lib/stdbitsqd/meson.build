# libstdbitsqd - Quadrate Standard Library - Bits Module
# Provides bit manipulation utilities

stdbitsqd_sources = files(
	'src/bits.c',
)

stdbitsqd_inc = include_directories('include')

# Shared library
stdbitsqd_shared = shared_library('stdbitsqd',
	stdbitsqd_sources,
	include_directories: stdbitsqd_inc,
	dependencies: qd_dep,
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdbitsqd_static = static_library('stdbitsqd_static',
	stdbitsqd_sources,
	include_directories: stdbitsqd_inc,
	dependencies: qd_static_dep,
	install: false
)

# Dependency declarations for other parts of the build
stdbitsqd_dep = declare_dependency(
	link_with: stdbitsqd_shared,
	include_directories: stdbitsqd_inc
)

stdbitsqd_static_dep = declare_dependency(
	link_with: stdbitsqd_static,
	include_directories: stdbitsqd_inc
)
