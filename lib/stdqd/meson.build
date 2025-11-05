# libstdqd - Quadrate Standard Library
# Provides networking, file I/O, and other standard utilities

stdqd_sources = files(
	'src/fmt.c',
	'src/net.c',
)

stdqd_inc = include_directories('include')

# Shared library
stdqd_shared = shared_library('stdqd',
	stdqd_sources,
	include_directories: [stdqd_inc, quadrate_inc, runtime_inc],
	link_with: quadrate_shared,
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdqd_static = static_library('stdqd_static',
	stdqd_sources,
	include_directories: [stdqd_inc, quadrate_inc, runtime_inc],
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
