stdbase64qd_sources = files(
	'src/base64.c',
)

stdbase64qd_inc = include_directories('include')

# Shared library
stdbase64qd_shared = shared_library('stdbase64qd',
	stdbase64qd_sources,
	include_directories: stdbase64qd_inc,
	dependencies: qdrt_dep,
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdbase64qd_static = static_library('stdbase64qd_static',
	stdbase64qd_sources,
	include_directories: stdbase64qd_inc,
	dependencies: qdrt_static_dep,
	install: false
)

stdbase64qd_dep = declare_dependency(
	link_with: stdbase64qd_shared,
	include_directories: stdbase64qd_inc
)

stdbase64qd_static_dep = declare_dependency(
	link_with: stdbase64qd_static,
	include_directories: stdbase64qd_inc
)
