# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_math = executable('test_math',
	'test_math.c',
	dependencies: [qdmath_dep, qdrt_static_dep, unit_check_dep],
	c_args: ['-Wno-sign-conversion'],
	build_by_default: true
)

test('test_math', test_math, timeout: 30)
