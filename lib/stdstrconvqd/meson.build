# libstdstrconvqd - Quadrate Standard Library - String Conversion Module
# Provides string-to-type and type-to-string conversions

stdstrconvqd_sources = files(
	'src/strconv.c',
)

stdstrconvqd_inc = include_directories('include')

# Shared library
stdstrconvqd_shared = shared_library('stdstrconvqd',
	stdstrconvqd_sources,
	include_directories: stdstrconvqd_inc,
	dependencies: qd_dep,
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdstrconvqd_static = static_library('stdstrconvqd_static',
	stdstrconvqd_sources,
	include_directories: stdstrconvqd_inc,
	dependencies: qd_static_dep,
	install: false
)

# Dependency declarations for other parts of the build
stdstrconvqd_dep = declare_dependency(
	link_with: stdstrconvqd_shared,
	include_directories: stdstrconvqd_inc
)

stdstrconvqd_static_dep = declare_dependency(
	link_with: stdstrconvqd_static,
	include_directories: stdstrconvqd_inc
)
