# libtty - Quadrate Standard Library - Terminal Detection Module

qdtty_sources = files(
	'src/tty.c',
)

# Native registration table, generated from the .qd import block
tty_register_c = custom_target('tty-register',
	input: files('qd/tty/tty.qd'),
	output: 'tty_register.c',
	command: [gen_module_register, '--module', 'tty', '@INPUT@', '@OUTPUT@']
)


if host_machine.system() == 'haiku'
	qdtty_sources += files('src/platform/haiku/tty_haiku.c')
else
	qdtty_sources += files('src/platform/posix/tty_posix.c')
endif

qdtty_inc = include_directories('include')

# Static library only
qdtty_static = static_library('tty',
	qdtty_sources,
	tty_register_c,
	include_directories: [qdtty_inc, qdrt_inc],
	dependencies: qdrt_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdtty_dep = declare_dependency(
	link_with: qdtty_static,
	include_directories: qdtty_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
