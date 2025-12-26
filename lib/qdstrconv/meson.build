# libqdstrconv - Quadrate Standard Library - String Conversion Module
# Provides string-to-type and type-to-string conversions

qdstrconv_sources = files(
	'src/strconv.c',
)

qdstrconv_inc = include_directories('include')

# Static library only
qdstrconv_static = static_library('qdstrconv',
	qdstrconv_sources,
	include_directories: qdstrconv_inc,
	dependencies: qd_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdstrconv_dep = declare_dependency(
	link_with: qdstrconv_static,
	include_directories: qdstrconv_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
