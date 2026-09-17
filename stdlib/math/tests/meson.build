# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_math = executable('test_math',
	'test_math.c',
	dependencies: [qdmath_dep, qdrt_static_dep, unit_check_dep],
	c_args: ['-Wno-sign-conversion'],
	build_by_default: true
)

test('test_math', test_math, timeout: 30)

# The generated registration table, driven through the interpreter
test_math_register = executable('test_math_register',
	'test_math_register.c',
	dependencies: [qdmath_dep, interp_dep, unit_check_dep],
	build_by_default: true
)

test('test_math_register', test_math_register, timeout: 30)
