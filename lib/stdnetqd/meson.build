# libstdnetqd - Quadrate Standard Library - Network Module
# Provides networking utilities

stdnetqd_sources = files(
	'src/net.c',
)

stdnetqd_inc = include_directories('include')

# Shared library
stdnetqd_shared = shared_library('stdnetqd',
	stdnetqd_sources,
	include_directories: stdnetqd_inc,
	dependencies: qd_dep,
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdnetqd_static = static_library('stdnetqd_static',
	stdnetqd_sources,
	include_directories: stdnetqd_inc,
	dependencies: qd_static_dep,
	install: false
)

# Dependency declarations for other parts of the build
stdnetqd_dep = declare_dependency(
	link_with: stdnetqd_shared,
	include_directories: stdnetqd_inc
)

stdnetqd_static_dep = declare_dependency(
	link_with: stdnetqd_static,
	include_directories: stdnetqd_inc
)
