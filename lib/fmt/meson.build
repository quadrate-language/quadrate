# libfmt - Quadrate Standard Library - Format Module
# Provides formatting utilities

qdfmt_sources = files(
	'src/fmt.c',
)

qdfmt_inc = include_directories('include')

# Static library only
qdfmt_static = static_library('fmt',
	qdfmt_sources,
	include_directories: qdfmt_inc,
	dependencies: qd_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdfmt_dep = declare_dependency(
	link_with: qdfmt_static,
	include_directories: qdfmt_inc
)
