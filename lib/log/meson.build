# liblog - Quadrate Standard Library - Logging Module
# Provides structured logging with levels and formatters

qdlog_sources = files(
	'src/log.c',
	'src/qd_log.c',
)

qdlog_inc = include_directories('include')

# Static library only
qdlog_static = static_library('log',
	qdlog_sources,
	include_directories: qdlog_inc,
	dependencies: [qdrt_dep],
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdlog_dep = declare_dependency(
	link_with: qdlog_static,
	include_directories: qdlog_inc
)

if get_option('build_tests')
	subdir('tests')
endif
