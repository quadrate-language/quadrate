# libstdfmtqd - Quadrate Standard Library - Format Module
# Provides formatting utilities

stdfmtqd_sources = files(
	'src/fmt.c',
)

stdfmtqd_inc = include_directories('include')

# Shared library
stdfmtqd_shared = shared_library('stdfmtqd',
	stdfmtqd_sources,
	include_directories: stdfmtqd_inc,
	dependencies: qd_dep,
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdfmtqd_static = static_library('stdfmtqd_static',
	stdfmtqd_sources,
	include_directories: stdfmtqd_inc,
	dependencies: qd_static_dep,
	install: false
)

# Dependency declarations for other parts of the build
stdfmtqd_dep = declare_dependency(
	link_with: stdfmtqd_shared,
	include_directories: stdfmtqd_inc
)

stdfmtqd_static_dep = declare_dependency(
	link_with: stdfmtqd_static,
	include_directories: stdfmtqd_inc
)
