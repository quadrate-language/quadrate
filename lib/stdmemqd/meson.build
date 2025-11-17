stdmemqd_sources = files(
	'src/mem.c',
)

stdmemqd_inc = include_directories('include')

# Shared library
stdmemqd_shared = shared_library('stdmemqd',
	stdmemqd_sources,
	include_directories: stdmemqd_inc,
	dependencies: qdrt_dep,
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdmemqd_static = static_library('stdmemqd_static',
	stdmemqd_sources,
	include_directories: stdmemqd_inc,
	dependencies: qdrt_static_dep,
	install: false
)

stdmemqd_dep = declare_dependency(
	link_with: stdmemqd_shared,
	include_directories: stdmemqd_inc
)

stdmemqd_static_dep = declare_dependency(
	link_with: stdmemqd_static,
	include_directories: stdmemqd_inc
)
