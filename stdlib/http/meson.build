# libhttp - Quadrate Standard Library - HTTP Module
# Provides HTTP client and server functionality

qdhttp_sources = files(
	'src/http.c',
	'src/http_server.c',
)

qdhttp_inc = include_directories('include')

# Static library only
qdhttp_static = static_library('http',
	qdhttp_sources,
	include_directories: qdhttp_inc,
	dependencies: [qdrt_dep],
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdhttp_dep = declare_dependency(
	link_with: qdhttp_static,
	include_directories: qdhttp_inc
)

if get_option('build_tests')
	subdir('tests')
endif
