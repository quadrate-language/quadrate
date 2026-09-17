# libstrconv - Quadrate Standard Library - String Conversion Module
# Provides string-to-type and type-to-string conversions

qdstrconv_sources = files(
	'src/strconv.c',
)

# Native registration table, generated from the .qd import block
strconv_register_c = custom_target('strconv-register',
	input: files('qd/strconv/strconv.qd'),
	output: 'strconv_register.c',
	command: [gen_module_register, '--module', 'strconv', '@INPUT@', '@OUTPUT@']
)


qdstrconv_inc = include_directories('include')

# Static library only
qdstrconv_static = static_library('strconv',
	qdstrconv_sources,
	strconv_register_c,
	include_directories: qdstrconv_inc,
	dependencies: qdrt_static_dep,
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
