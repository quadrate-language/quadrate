# libtls - Quadrate Standard Library - TLS Module
# Provides TLS/SSL encryption on top of TCP sockets

cc = meson.get_compiler('c')
openssl_dep = cc.find_library('ssl')
crypto_dep = cc.find_library('crypto')

qdtls_sources = files(
	'src/tls.c',
	'src/platform/openssl/tls_openssl.c',
)

# Native registration table, generated from the .qd import block
tls_register_c = custom_target('tls-register',
	input: files('qd/tls/tls.qd'),
	output: 'tls_register.c',
	command: [gen_module_register, '--module', 'tls', '@INPUT@', '@OUTPUT@']
)


qdtls_inc = include_directories('include', 'src')

# Static library only
qdtls_static = static_library('tls',
	qdtls_sources,
	tls_register_c,
	include_directories: qdtls_inc,
	dependencies: [qdrt_static_dep, openssl_dep, crypto_dep],
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdtls_dep = declare_dependency(
	link_with: qdtls_static,
	include_directories: qdtls_inc
)
