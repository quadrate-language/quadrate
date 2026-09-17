# liblog - Quadrate Standard Library - Logging Module
# Provides structured logging with levels and formatters

qdlog_sources = files(
	'src/log.c',
	'src/qd_log.c',
)

# Native registration table, generated from the .qd import block
log_register_c = custom_target('log-register',
	input: files('qd/log/log.qd'),
	output: 'log_register.c',
	command: [gen_module_register, '--module', 'log', '@INPUT@', '@OUTPUT@']
)


qdlog_inc = include_directories('include')

# Static library only
qdlog_static = static_library('log',
	qdlog_sources,
	log_register_c,
	include_directories: qdlog_inc,
	dependencies: [qdrt_static_dep],
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
