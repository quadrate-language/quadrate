# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_testing = executable('test_testing',
	'test_testing.c',
	dependencies: [qdtesting_dep, qdrt_static_dep, unit_check_dep],
	c_args: ['-Wno-sign-conversion'],
	build_by_default: true
)

test('test_testing', test_testing, timeout: 30)
