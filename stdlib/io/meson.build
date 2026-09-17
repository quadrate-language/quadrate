# libio - Quadrate Standard Library - File I/O Module

# pthread needed for thread-safe file registry
threads_dep_io = dependency('threads', required: true)

qdio_sources = files(
	'src/io.c',
)

# Native registration table, generated from the .qd import block
io_register_c = custom_target('io-register',
	input: files('qd/io/io.qd'),
	output: 'io_register.c',
	command: [gen_module_register, '--module', 'io', '@INPUT@', '@OUTPUT@']
)


qdio_inc = include_directories('include')

# Static library only
qdio_static = static_library('io',
	qdio_sources,
	io_register_c,
	include_directories: qdio_inc,
	dependencies: [qdrt_static_dep, threads_dep_io],
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdio_dep = declare_dependency(
	link_with: qdio_static,
	include_directories: qdio_inc,
	dependencies: [threads_dep_io]
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
