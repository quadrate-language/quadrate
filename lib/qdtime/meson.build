# libqdtime - Quadrate Standard Library - Time Module
# Provides time utilities

qdtime_sources = files(
	'src/time.c',
	'src/platform/posix/time_posix.c',
)

qdtime_inc = include_directories('include')

# Static library only
qdtime_static = static_library('qdtime',
	qdtime_sources,
	include_directories: qdtime_inc,
	dependencies: qd_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdtime_dep = declare_dependency(
	link_with: qdtime_static,
	include_directories: qdtime_inc
)
