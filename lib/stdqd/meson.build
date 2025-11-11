# libstdqd - Quadrate Standard Library
# Provides networking, file I/O, and other standard utilities

stdqd_sources = files(
	'src/bits.c',
	'src/fmt.c',
	'src/math.c',
	'src/net.c',
	'src/str.c',
	'src/time.c',
)

stdqd_inc = include_directories('include')

# Shared library
stdqd_shared = shared_library('stdqd',
	stdqd_sources,
	include_directories: stdqd_inc,
	dependencies: qd_dep,
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdqd_static = static_library('stdqd_static',
	stdqd_sources,
	include_directories: stdqd_inc,
	dependencies: qd_static_dep,
	install: false
)

# Dependency declarations for other parts of the build
stdqd_dep = declare_dependency(
	link_with: stdqd_shared,
	include_directories: stdqd_inc
)

stdqd_static_dep = declare_dependency(
	link_with: stdqd_static,
	include_directories: stdqd_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
