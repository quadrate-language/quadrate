# libstrings - Quadrate Standard Library - String Module
# Provides string utilities

qdstrings_sources = files(
	'src/strings.c',
)

qdstrings_inc = include_directories('include')

# Static library only
qdstrings_static = static_library('strings',
	qdstrings_sources,
	include_directories: qdstrings_inc,
	dependencies: qd_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdstrings_dep = declare_dependency(
	link_with: qdstrings_static,
	include_directories: qdstrings_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
