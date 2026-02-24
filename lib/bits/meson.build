# libbits - Quadrate Standard Library - Bits Module
# Provides bit manipulation utilities

qdbits_sources = files(
	'src/bits.c',
)

qdbits_inc = include_directories('include')

# Static library only
qdbits_static = static_library('bits',
	qdbits_sources,
	include_directories: qdbits_inc,
	dependencies: qd_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdbits_dep = declare_dependency(
	link_with: qdbits_static,
	include_directories: qdbits_inc
)
