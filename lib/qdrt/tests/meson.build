# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_runtime = executable('test_runtime',
	'test_runtime.c',
	dependencies: [qdrt_static_dep, unit_check_dep],
	c_args: ['-Wno-sign-conversion'],
	build_by_default: true
)

test('test_runtime', test_runtime)
