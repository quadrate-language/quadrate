# libstdtimeqd - Quadrate Standard Library - Time Module
# Provides time utilities

stdtimeqd_sources = files(
	'src/time.c',
)

stdtimeqd_inc = include_directories('include')

# Shared library
stdtimeqd_shared = shared_library('stdtimeqd',
	stdtimeqd_sources,
	include_directories: stdtimeqd_inc,
	dependencies: qd_dep,
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdtimeqd_static = static_library('stdtimeqd_static',
	stdtimeqd_sources,
	include_directories: stdtimeqd_inc,
	dependencies: qd_static_dep,
	install: false
)

# Dependency declarations for other parts of the build
stdtimeqd_dep = declare_dependency(
	link_with: stdtimeqd_shared,
	include_directories: stdtimeqd_inc
)

stdtimeqd_static_dep = declare_dependency(
	link_with: stdtimeqd_static,
	include_directories: stdtimeqd_inc
)
