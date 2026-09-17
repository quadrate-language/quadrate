# libfmt - Quadrate Standard Library - Format Module
# Provides formatting utilities

qdfmt_sources = files(
	'src/fmt.c',
)

# Native registration table, generated from the .qd import block
fmt_register_c = custom_target('fmt-register',
	input: files('qd/fmt/fmt.qd'),
	output: 'fmt_register.c',
	command: [gen_module_register, '--module', 'fmt', '@INPUT@', '@OUTPUT@']
)


qdfmt_inc = include_directories('include')

# Static library only
qdfmt_static = static_library('fmt',
	qdfmt_sources,
	fmt_register_c,
	include_directories: qdfmt_inc,
	dependencies: qdrt_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdfmt_dep = declare_dependency(
	link_with: qdfmt_static,
	include_directories: qdfmt_inc
)

if get_option('build_tests')
	subdir('tests')
endif
