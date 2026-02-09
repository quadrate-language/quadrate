# libqdnet - Quadrate Standard Library - Network Socket Module

qdnet_sources = files(
	'src/net.c',
	'src/platform/posix/net_socket.c',
)

qdnet_inc = include_directories('include')

qdnet_link_args = []
if host_machine.system() == 'haiku'
	cc = meson.get_compiler('c')
	qdnet_network_dep = cc.find_library('network')
	qdnet_link_args += ['-lnetwork']
endif

# Static library only
qdnet_static = static_library('qdnet',
	qdnet_sources,
	include_directories: [qdnet_inc, qdrt_inc],
	dependencies: qdrt_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdnet_dep = declare_dependency(
	link_with: qdnet_static,
	include_directories: qdnet_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
