# libqdtls - Quadrate Standard Library - TLS Module
# Provides TLS/SSL encryption using OpenSSL

# Find OpenSSL
openssl_dep = dependency('openssl', version: '>=1.1.0', required: true)

qdtls_sources = files(
	'src/tls.c',
	'src/platform/openssl/tls_openssl.c',
)

qdtls_inc = include_directories('include')

# Static library
qdtls_static = static_library('qdtls',
	qdtls_sources,
	include_directories: qdtls_inc,
	dependencies: [qd_static_dep, openssl_dep],
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdtls_dep = declare_dependency(
	link_with: qdtls_static,
	include_directories: qdtls_inc,
	dependencies: [openssl_dep]
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
