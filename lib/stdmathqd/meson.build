# libstdmathqd - Quadrate Standard Library - Math Module
# Provides mathematical functions

stdmathqd_sources = files(
	'src/math.c',
)

stdmathqd_inc = include_directories('include')

# Shared library
stdmathqd_shared = shared_library('stdmathqd',
	stdmathqd_sources,
	include_directories: stdmathqd_inc,
	dependencies: qd_dep,
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdmathqd_static = static_library('stdmathqd_static',
	stdmathqd_sources,
	include_directories: stdmathqd_inc,
	dependencies: qd_static_dep,
	install: false
)

# Dependency declarations for other parts of the build
stdmathqd_dep = declare_dependency(
	link_with: stdmathqd_shared,
	include_directories: stdmathqd_inc
)

stdmathqd_static_dep = declare_dependency(
	link_with: stdmathqd_static,
	include_directories: stdmathqd_inc
)
