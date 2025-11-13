# libstdosqd - Quadrate Standard Library - OS Module
# Provides operating system utilities

stdosqd_sources = files(
	'src/os.c',
)

stdosqd_inc = include_directories('include')

# Shared library
stdosqd_shared = shared_library('stdosqd',
	stdosqd_sources,
	include_directories: stdosqd_inc,
	dependencies: qd_dep,
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdosqd_static = static_library('stdosqd_static',
	stdosqd_sources,
	include_directories: stdosqd_inc,
	dependencies: qd_static_dep,
	install: false
)

# Dependency declarations for other parts of the build
stdosqd_dep = declare_dependency(
	link_with: stdosqd_shared,
	include_directories: stdosqd_inc
)

stdosqd_static_dep = declare_dependency(
	link_with: stdosqd_static,
	include_directories: stdosqd_inc
)
