# libdecimal - Quadrate Standard Library - decimal Module
# The IEEE 754 decimal128 operations beyond the operators, and the elementary functions

qddecimal_sources = files(
	'src/decimal.c',
)

# Native registration table, generated from the .qd import block
decimal_register_c = custom_target('decimal-register',
	input: files('qd/decimal/decimal.qd'),
	output: 'decimal_register.c',
	command: [gen_module_register, '--module', 'decimal', '@INPUT@', '@OUTPUT@']
)


qddecimal_inc = include_directories('include')

# Static library only. The decimal arithmetic itself is the runtime's, so this needs
# nothing beyond it.
qddecimal_static = static_library('decimal',
	qddecimal_sources,
	decimal_register_c,
	include_directories: qddecimal_inc,
	dependencies: qdrt_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qddecimal_dep = declare_dependency(
	link_with: qddecimal_static,
	include_directories: qddecimal_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
