# libqdbits - Quadrate Standard Library - Bits Module
# Provides bit manipulation utilities

qdbits_sources = files(
	'src/bits.c',
)

qdbits_inc = include_directories('include')

# Static library only
qdbits_static = static_library('qdbits',
	qdbits_sources,
	include_directories: qdbits_inc,
	dependencies: qd_static_dep,
	install: false
)

# Dependency declaration for other parts of the build
qdbits_dep = declare_dependency(
	link_with: qdbits_static,
	include_directories: qdbits_inc
)
