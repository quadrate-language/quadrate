# libnet - Quadrate Standard Library - Network Socket Module

qdnet_sources = files(
	'src/net.c',
	'src/platform/posix/net_socket.c',
)

# Native registration table, generated from the .qd import block
net_register_c = custom_target('net-register',
	input: files('qd/net/net.qd'),
	output: 'net_register.c',
	command: [gen_module_register, '--module', 'net', '@INPUT@', '@OUTPUT@']
)


qdnet_inc = include_directories('include')

qdnet_extra_deps = []
if host_machine.system() == 'haiku'
	cc = meson.get_compiler('c')
	qdnet_extra_deps += cc.find_library('network')
endif

# Static library only
qdnet_static = static_library('net',
	qdnet_sources,
	net_register_c,
	include_directories: [qdnet_inc, qdrt_inc],
	dependencies: [qdrt_static_dep] + qdnet_extra_deps,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdnet_dep = declare_dependency(
	link_with: qdnet_static,
	include_directories: qdnet_inc,
	dependencies: qdnet_extra_deps
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
