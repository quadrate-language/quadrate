unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_http = executable('test_http',
	'test_http.c',
	dependencies: [qdhttp_dep, qdnet_dep, qdtls_dep, qdrt_static_dep, unit_check_dep],
	c_args: ['-Wno-sign-conversion'],
	build_by_default: true
)

test('test_http', test_http, timeout: 30)
