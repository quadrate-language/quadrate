# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_str = executable('test_str',
	'test_str.c',
	dependencies: [qdstr_dep, qdrt_static_dep, unit_check_dep],
	c_args: ['-Wno-sign-conversion'],
	build_by_default: true
)

test('test_str', test_str, timeout: 30)
