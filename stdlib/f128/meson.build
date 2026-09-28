# libf128 - Quadrate Standard Library - f128 Module
# The IEEE 754 decimal128 operations beyond the operators, and the elementary functions

qdf128_sources = files(
	'src/f128.c',
)

# Native registration table, generated from the .qd import block
f128_register_c = custom_target('f128-register',
	input: files('qd/f128/f128.qd'),
	output: 'f128_register.c',
	command: [gen_module_register, '--module', 'f128', '@INPUT@', '@OUTPUT@']
)


qdf128_inc = include_directories('include')

# Static library only. The decimal arithmetic itself is the runtime's, so this needs
# nothing beyond it.
qdf128_static = static_library('f128',
	qdf128_sources,
	f128_register_c,
	include_directories: qdf128_inc,
	dependencies: qdrt_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdf128_dep = declare_dependency(
	link_with: qdf128_static,
	include_directories: qdf128_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
