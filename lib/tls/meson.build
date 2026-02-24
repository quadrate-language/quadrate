# libtls - Quadrate Standard Library - TLS Module
# Provides TLS/SSL encryption on top of TCP sockets

cc = meson.get_compiler('c')
openssl_dep = cc.find_library('ssl')
crypto_dep = cc.find_library('crypto')

qdtls_sources = files(
	'src/tls.c',
	'src/platform/openssl/tls_openssl.c',
)

qdtls_inc = include_directories('include', 'src')

# Static library only
qdtls_static = static_library('tls',
	qdtls_sources,
	include_directories: qdtls_inc,
	dependencies: [qdrt_dep, openssl_dep, crypto_dep],
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdtls_dep = declare_dependency(
	link_with: qdtls_static,
	include_directories: qdtls_inc
)
