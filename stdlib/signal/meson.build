# libsignal - Quadrate Standard Library - Signal Module
# Provides Unix signal handling with polling-based API

qdsignal_sources = files(
	'src/signal.c',
)

qdsignal_inc = include_directories('include')

# Static library only
qdsignal_static = static_library('signal',
	qdsignal_sources,
	include_directories: qdsignal_inc,
	dependencies: [qdrt_static_dep],
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdsignal_dep = declare_dependency(
	link_with: qdsignal_static,
	include_directories: qdsignal_inc
)

if get_option('build_tests')
	subdir('tests')
endif
