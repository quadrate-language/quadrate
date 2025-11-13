# libstdstrqd - Quadrate Standard Library - String Module
# Provides string utilities

stdstrqd_sources = files(
	'src/str.c',
)

stdstrqd_inc = include_directories('include')

# Shared library
stdstrqd_shared = shared_library('stdstrqd',
	stdstrqd_sources,
	include_directories: stdstrqd_inc,
	dependencies: qd_dep,
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdstrqd_static = static_library('stdstrqd_static',
	stdstrqd_sources,
	include_directories: stdstrqd_inc,
	dependencies: qd_static_dep,
	install: false
)

# Dependency declarations for other parts of the build
stdstrqd_dep = declare_dependency(
	link_with: stdstrqd_shared,
	include_directories: stdstrqd_inc
)

stdstrqd_static_dep = declare_dependency(
	link_with: stdstrqd_static,
	include_directories: stdstrqd_inc
)
