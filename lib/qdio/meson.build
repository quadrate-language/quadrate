# libqdio - Quadrate Standard Library - File I/O Module

qdio_sources = files(
	'src/io.c',
)

qdio_inc = include_directories('include')

# Static library only
qdio_static = static_library('qdio',
	qdio_sources,
	include_directories: qdio_inc,
	dependencies: qd_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdio_dep = declare_dependency(
	link_with: qdio_static,
	include_directories: qdio_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
