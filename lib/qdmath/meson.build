# libqdmath - Quadrate Standard Library - Math Module
# Provides mathematical functions

qdmath_sources = files(
	'src/math.c',
)

qdmath_inc = include_directories('include')

# Static library only
qdmath_static = static_library('qdmath',
	qdmath_sources,
	include_directories: qdmath_inc,
	dependencies: qd_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdmath_dep = declare_dependency(
	link_with: qdmath_static,
	include_directories: qdmath_inc
)
