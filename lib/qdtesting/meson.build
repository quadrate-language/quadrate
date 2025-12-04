# libqdtesting - Quadrate Standard Library - Testing Module
# Provides assertion functions for unit testing

qdtesting_sources = files(
	'src/testing.c',
)

qdtesting_inc = include_directories('include')

# Static library only
qdtesting_static = static_library('qdtesting',
	qdtesting_sources,
	include_directories: qdtesting_inc,
	dependencies: qdrt_static_dep,
	install: false
)

# Dependency declaration for other parts of the build
qdtesting_dep = declare_dependency(
	link_with: qdtesting_static,
	include_directories: qdtesting_inc
)
