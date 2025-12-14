# libqdnet - Quadrate Standard Library - Network Module
# Provides networking utilities

qdnet_sources = files(
	'src/net.c',
	'src/platform/posix/net_socket.c',
)

qdnet_inc = include_directories('include')

# Static library only
qdnet_static = static_library('qdnet',
	qdnet_sources,
	include_directories: qdnet_inc,
	dependencies: qd_static_dep,
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
