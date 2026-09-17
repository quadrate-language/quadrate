# libtime - Quadrate Standard Library - Time Module
# Provides time utilities

qdtime_sources = files(
	'src/time.c',
	'src/platform/posix/time_posix.c',
)

# Native registration table, generated from the .qd import block
time_register_c = custom_target('time-register',
	input: files('qd/time/time.qd'),
	output: 'time_register.c',
	command: [gen_module_register, '--module', 'time', '@INPUT@', '@OUTPUT@']
)


qdtime_inc = include_directories('include')

# Static library only
qdtime_static = static_library('time',
	qdtime_sources,
	time_register_c,
	include_directories: qdtime_inc,
	dependencies: qdrt_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdtime_dep = declare_dependency(
	link_with: qdtime_static,
	include_directories: qdtime_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
