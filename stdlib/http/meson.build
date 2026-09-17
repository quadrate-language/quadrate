# libhttp - Quadrate Standard Library - HTTP Module
# Provides HTTP client and server functionality

qdhttp_sources = files(
	'src/http.c',
	'src/http_server.c',
)

# Native registration table, generated from the .qd import block
http_register_c = custom_target('http-register',
	input: files('qd/http/http.qd'),
	output: 'http_register.c',
	command: [gen_module_register, '--module', 'http', '@INPUT@', '@OUTPUT@']
)


qdhttp_inc = include_directories('include')

# Static library only
qdhttp_static = static_library('http',
	qdhttp_sources,
	http_register_c,
	include_directories: qdhttp_inc,
	dependencies: [qdrt_static_dep],
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
