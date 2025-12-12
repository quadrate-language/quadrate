# libqdmem - Quadrate Standard Library - Memory Module

qdmem_sources = files(
	'src/mem.c',
)

qdmem_inc = include_directories('include')

# Static library only
qdmem_static = static_library('qdmem',
	qdmem_sources,
	include_directories: qdmem_inc,
	dependencies: qdrt_static_dep,
	install: false
)

# Dependency declaration for other parts of the build
qdmem_dep = declare_dependency(
	link_with: qdmem_static,
	include_directories: qdmem_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
